#include "ExecutionEngine.h"

ExecutionEngine::ExecutionEngine(RPMReader& rpm, ESCController& esc, SafetyManager& safety, ProfileManager& pm, EEPROMStorage& storage)
    : _rpmReader(rpm), _escController(esc), _safetyManager(safety), _profileManager(pm), _storage(storage),
      _state(STATE_IDLE), _currentStepIndex(0), _stepStartTime(0), _pauseStartTime(0), _currentTargetRPM(0), _manualTargetRPM(0) {
}

void ExecutionEngine::begin() {
    _state = STATE_IDLE;
    _escController.stopMotor();
}

void ExecutionEngine::update() {
    // 1. Read Sensors
    float currentRPM = _rpmReader.getRPM();
    
    // 2. Safety Check 
    // (Always active unless in IDLE/ERROR to prevent lockup)
    if (_state != STATE_IDLE && _state != STATE_ERROR && _state != STATE_EMERGENCY_STOP && _state != STATE_TUNING && _state != STATE_CALIBRATING) {
        // We pass a dummy throttle value or rely on SafetyManager to check RPM vs Target
        if (!_safetyManager.check(currentRPM, _currentTargetRPM, _escController.getThrottleMicroseconds())) { 
            _state = STATE_EMERGENCY_STOP;
        }
    }

    // 3. State Machine
    switch (_state) {
        case STATE_IDLE:
            _escController.stopMotor();
            break;

        case STATE_RUNNING: {
            unsigned long now = millis();
            unsigned long elapsed = now - _stepStartTime;
            
            SpinStep& step = _currentProfile.steps[_currentStepIndex];
            
            if (elapsed < step.rampDurationMs) {
                // Ramping
                _currentTargetRPM = calculateRampRPM(step, elapsed);
            } else if (elapsed < (step.rampDurationMs + step.holdDurationMs)) {
                // Holding
                _currentTargetRPM = step.targetRPM;
            } else {
                // Step Complete
                advanceStep();
                return; // Wait for next cycle
            }
            
            _escController.setTargetRPM(_currentTargetRPM);
            _escController.update(currentRPM);
            break;
        }

        case STATE_PAUSED:
            // Maintain current RPM
            _escController.setTargetRPM(_currentTargetRPM);
            _escController.update(currentRPM);
            break;

        case STATE_STOPPING:
            _escController.stopMotor();
            if (currentRPM < 100) {
                _state = STATE_IDLE;
            }
            break;

        case STATE_TUNING:
            if (_escController.updateTuning(currentRPM)) {
                // Tuning Complete
                PIDConstants newPid = _escController.getTunedPID();
                _escController.setPID(newPid.kp, newPid.ki, newPid.kd);
                
                // Save to EEPROM
                SystemSettings s;
                _storage.loadSettings(s);
                s.pid = newPid;
                _storage.saveSettings(s);
                
                _state = STATE_IDLE;
                Serial.println("Tuning Complete. Saved.");
            }
            break;

        case STATE_MANUAL:
            _currentTargetRPM = _manualTargetRPM;
            _escController.setTargetRPM(_currentTargetRPM);
            _escController.update(currentRPM);
            break;

        case STATE_CALIBRATING:
            // Throttle is set directly by setCalibrationThrottle, no PID or ramp
            break;

        case STATE_ERROR:
        case STATE_EMERGENCY_STOP:
            _escController.stopMotor();
            break;
    }
}

void ExecutionEngine::runProfile(uint8_t profileId) {
    if (_profileManager.getProfile(profileId, _currentProfile)) {
        _state = STATE_RUNNING;
        _currentStepIndex = 0;
        _stepStartTime = millis();
        // Initialize first step's startRPM to the current measured RPM so ramps chain correctly
        float currentRPM = _rpmReader.getRPM();
        if (_currentProfile.stepCount > 0) {
            _currentProfile.steps[0].startRPM = (int)currentRPM;
        }
        _currentTargetRPM = 0;
        _safetyManager.clearError();
        Serial.print("Engine: Started Profile "); Serial.println(_currentProfile.name);
    } else {
        Serial.print("Engine: Failed to load profile ID "); Serial.println(profileId);
    }
}

void ExecutionEngine::stop() {
    _state = STATE_STOPPING;
}

void ExecutionEngine::pause() {
    if (_state == STATE_RUNNING) {
        _state = STATE_PAUSED;
        _pauseStartTime = millis();
    }
}

void ExecutionEngine::resume() {
    if (_state == STATE_PAUSED) {
        _state = STATE_RUNNING;
        // Adjust start time to account for pause duration
        unsigned long pauseDuration = millis() - _pauseStartTime;
        _stepStartTime += pauseDuration;
    }
}

void ExecutionEngine::startTuning() {
    if (_state == STATE_IDLE) {
        _state = STATE_TUNING;
        _escController.startTuning();
    }
}

void ExecutionEngine::startManual() {
    _state = STATE_MANUAL;
    _manualTargetRPM = 0;
    _escController.setTargetRPM(0);
}

void ExecutionEngine::setManualRPM(float rpm) {
    if (_state == STATE_MANUAL) {
        _manualTargetRPM = rpm;
    }
}

void ExecutionEngine::setCalibrationThrottle(int us) {
    _state = STATE_CALIBRATING;
    _escController.setThrottleMicroseconds(us);
}

void ExecutionEngine::advanceStep() {
    // Capture previous target so next step can start from it
    int prevTarget = _currentProfile.steps[_currentStepIndex].targetRPM;
    _currentStepIndex++;
    if (_currentStepIndex >= _currentProfile.stepCount) {
        // Profile Complete
        stop();
    } else {
        // Ensure the next step's startRPM equals the previous step's targetRPM
        _currentProfile.steps[_currentStepIndex].startRPM = prevTarget;
        _stepStartTime = millis();
    }
}

float ExecutionEngine::calculateRampRPM(const SpinStep& step, unsigned long elapsed) {
    float t = (float)elapsed / (float)step.rampDurationMs;
    if (t > 1.0f) t = 1.0f;
    
    float start = (float)step.startRPM;
    float end = (float)step.targetRPM;
    float delta = end - start;
    
    switch (step.rampType) {
        case RAMP_LINEAR:
            return start + (delta * t);
        case RAMP_EXPONENTIAL:
            return start + (delta * (t * t)); // Simple ease-in
        case RAMP_S_CURVE:
            return start + delta * (1.0f - cos(t * PI)) / 2.0f;
        default:
            return start + (delta * t);
    }
}

TelemetryData ExecutionEngine::getTelemetry() {
    TelemetryData data;
    data.state = _state;
    data.currentRPM = _rpmReader.getRPM();
    data.targetRPM = _currentTargetRPM;
    data.throttlePercent = _escController.getThrottlePercent();
    data.currentStepIndex = _currentStepIndex;
    
    if (_state == STATE_RUNNING) {
        unsigned long elapsed = millis() - _stepStartTime;
        unsigned long totalStepTime = _currentProfile.steps[_currentStepIndex].rampDurationMs + 
                                      _currentProfile.steps[_currentStepIndex].holdDurationMs;
        if (totalStepTime > elapsed)
            data.stepTimeRemaining = totalStepTime - elapsed;
        else 
            data.stepTimeRemaining = 0;
    } else {
        data.stepTimeRemaining = 0;
    }
    
    data.errorMessage = _safetyManager.getLastError();
    data.profileId = _currentProfile.id;
    return data;
}