#include "ExecutionEngine.h"

ExecutionEngine::ExecutionEngine(RPMReader& rpm, ESCController& esc, SafetyManager& safety, ProfileManager& pm, EEPROMStorage& storage)
    : _rpmReader(rpm), _escController(esc), _safetyManager(safety), _profileManager(pm), _storage(storage),
      _state(STATE_IDLE), _currentStepIndex(0), _stepStartTime(0), _pauseStartTime(0), 
      _currentTargetRPM(0), _manualTargetRPM(0), _mappingPulseWidth(0), _mappingEndPulse(0), 
      _lastRecordedPwm(0), _mappingStep(20), _isMapPointRecorded(false),
      _sumX(0), _sumY(0), _sumXY(0), _sumX2(0), _mappingCount(0),
      _btnStartPressed(false), _btnStopPressed(false), _lastBtnStart(false) {
    // Mapping variables initialized in member initializer list above
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
    if (_state != STATE_IDLE && _state != STATE_ERROR && _state != STATE_EMERGENCY_STOP && _state != STATE_TUNING) {
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
            
            if (step.rampType == RAMP_WAIT_BUTTON) {
                _currentTargetRPM = 0;
                _escController.setTargetRPM(0);
                _escController.update(currentRPM);
                
                // Detect a new press (rising edge of the pressed state)
                if (_btnStartPressed && !_lastBtnStart) {
                    advanceStep();
                }
                _lastBtnStart = _btnStartPressed;
                return;
            }

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

        case STATE_MAPPING: {
            unsigned long now = millis();
            _escController.setThrottleMicroseconds(_mappingPulseWidth);

            // Log new PWM value being tested
            static int lastLoggedPWM = -1;
            if (lastLoggedPWM != _mappingPulseWidth) {
                Serial.print("Mapping: Applying "); Serial.print(_mappingPulseWidth);
                Serial.println("us. Waiting 5s for stabilization...");
                lastLoggedPWM = _mappingPulseWidth;
            }
            
            // Wait 5 seconds for RPM to stabilize at this PWM value
            if (now - _stepStartTime >= 5000) {
                Serial.print("Mapping Point Captured -> PWM: "); Serial.print(_mappingPulseWidth);
                Serial.print("us | Final RPM: "); Serial.println(currentRPM);

                // Only include points where the motor is actually spinning for the line fit
                if (currentRPM > 1.0f) {
                    _sumX += _mappingPulseWidth;
                    _sumY += currentRPM;
                    _sumXY += ((double)_mappingPulseWidth * currentRPM);
                    _sumX2 += ((double)_mappingPulseWidth * _mappingPulseWidth);
                    _mappingCount++;
                }

                _lastRecordedPwm = _mappingPulseWidth; // Lock the PWM value used for this measurement
                _isMapPointRecorded = true;
                _mappingPulseWidth += _mappingStep;
                _stepStartTime = now;

                if (_mappingPulseWidth > _mappingEndPulse) {
                    Serial.println("Mapping Process Complete. Calculating Fit...");
                    
                    if (_mappingCount >= 2) {
                        float slope = (_mappingCount * _sumXY - _sumX * _sumY) / (_mappingCount * _sumX2 - _sumX * _sumX);
                        float intercept = (_sumY - slope * _sumX) / _mappingCount;
                        int inferredStart = (int)(-intercept / slope);

                        SystemSettings s;
                        _storage.loadSettings(s);
                        s.mapSlope = slope;
                        s.mapIntercept = intercept;
                        s.mapStartPWM = inferredStart;
                        
                        // Push new empirical parameters to the controller immediately
                        _escController.setMappingParams(slope, inferredStart);
                        
                        _storage.saveSettings(s);
                        Serial.print("Fit Result: Slope="); Serial.print(slope); Serial.print(" StartPWM="); Serial.println(inferredStart);
                    }
                    stop();
                }
            }
            // We don't use PID during raw PWM mapping
            _currentTargetRPM = 0; 
            break;
        }

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

void ExecutionEngine::startPwmMapping(int start, int end, int step) {
    _state = STATE_MAPPING;
    _mappingPulseWidth = start;
    _mappingEndPulse = end;
    _mappingStep = (step > 0) ? step : 20;
    _sumX = 0;
    _sumY = 0;
    _sumXY = 0;
    _sumX2 = 0;
    _mappingCount = 0;
    _stepStartTime = millis();
}


void ExecutionEngine::setManualRPM(float rpm) {
    if (_state == STATE_MANUAL) {
        _manualTargetRPM = rpm;
    }
}

void ExecutionEngine::setButtonStates(bool startPressed, bool stopPressed) {
    _btnStartPressed = startPressed;
    _btnStopPressed = stopPressed;
    // Note: _lastBtnStart is handled inside update() to ensure 
    // edge detection happens relative to the state machine cycle.
}

void ExecutionEngine::startManual() {
    _state = STATE_MANUAL;
    _manualTargetRPM = 0;
    _escController.setTargetRPM(0);
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
    data.btnStartPressed = false;
    data.btnStopPressed = false;
    
    // Use the locked mapping PWM if this is a recorded point, 
    // otherwise use live throttle
    if (_isMapPointRecorded) {
        data.pulseWidth = _lastRecordedPwm;
    } else {
        data.pulseWidth = _escController.getThrottleMicroseconds();
    }

    data.isMapPoint = _isMapPointRecorded;
    _isMapPointRecorded = false; // Reset flag after reporting
    
    if (_state == STATE_RUNNING) {
        unsigned long elapsed = millis() - _stepStartTime;
        unsigned long totalStepTime = _currentProfile.steps[_currentStepIndex].rampDurationMs + 
                                      _currentProfile.steps[_currentStepIndex].holdDurationMs;
        data.stepTotalTime = totalStepTime;
        if (totalStepTime > elapsed)
            data.stepTimeRemaining = totalStepTime - elapsed;
        else 
            data.stepTimeRemaining = 0;

        // Calculate Total Profile Times
        unsigned long totalDur = 0;
        unsigned long remDur = 0;
        for (int i = 0; i < _currentProfile.stepCount; i++) {
            unsigned long stepDur = _currentProfile.steps[i].rampDurationMs + _currentProfile.steps[i].holdDurationMs;
            totalDur += stepDur;
            if (i > _currentStepIndex) {
                remDur += stepDur;
            } else if (i == _currentStepIndex) {
                remDur += data.stepTimeRemaining;
            }
        }
        data.totalProfileTime = totalDur;
        data.totalTimeRemaining = remDur;
    } else {
        data.stepTimeRemaining = 0;
        data.stepTotalTime = 0;
        data.totalTimeRemaining = 0;
        data.totalProfileTime = 0;
    }
    
    data.errorMessage = _safetyManager.getLastError();
    data.profileId = _currentProfile.id;
    return data;
}