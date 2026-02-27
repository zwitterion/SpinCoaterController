#include "ESCController.h"

ESCController::ESCController(uint8_t pin) : _pin(pin), _targetRPM(0), 
    _kp(0), _ki(0), _kd(0), _integral(0), _prevError(0), _lastPIDTime(0),
    _minUs(1000), _maxUs(2000), _filterAlpha(1.0), _windupRange(500), _filteredRPM(0),
    _tuneState(TUNE_IDLE), _lastThrottle(1000), _controlMode(CONTROL_PID), _motorKV(850.0f), _batteryVoltage(11.1f) {
}

void ESCController::begin() {
    _esc.attach(_pin, _minUs, _maxUs);
    stopMotor(); // Send min throttle to arm ESC
}

void ESCController::setThrottleMicroseconds(int us) {
    // Safety clamp
    us = constrain(us, _minUs, _maxUs);
    _lastThrottle = us;
    _esc.writeMicroseconds(us);
}

int ESCController::getThrottleMicroseconds() {
    return _lastThrottle;
}

void ESCController::setTargetRPM(float rpm) {
    if (rpm <= 0) {
        stopMotor();
    } else {
        // If starting from idle, reset PID timers to prevent integral windup from stale time
        if (_controlMode == CONTROL_KV) {
            _targetRPM = rpm;
            // Open-loop: compute throttle percent from KV * Vbat
            float denom = _motorKV * _batteryVoltage;
            float pct = 0.0f;
            if (denom > 0.0f) {
                pct = rpm / denom;
            }
            pct = constrain(pct, 0.0f, 1.0f);
            int us = _minUs + (int)(pct * (_maxUs - _minUs));
            // Serial.print("KV Mode: Target RPM = "); Serial.print(rpm);
            // Serial.print(", Throttle % = "); Serial.print(pct * 100.0f);
            // Serial.print(", Throttle us = "); Serial.println(us);
            setThrottleMicroseconds(us);
        } else {
            if (_targetRPM <= 0) {
                _lastPIDTime = millis();
                _integral = 0;
                _prevError = 0;
            }
            _targetRPM = rpm;
        }
    }
}

void ESCController::stopMotor() {
    _targetRPM = 0;
    _integral = 0;
    _prevError = 0;
    setThrottleMicroseconds(_minUs);
}

void ESCController::setPID(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void ESCController::setCalibration(int minUs, int maxUs) {
    _minUs = minUs;
    _maxUs = maxUs;
}

void ESCController::setFilterAlpha(float alpha) {
    _filterAlpha = constrain(alpha, 0.01f, 1.0f);
}

void ESCController::setWindupRange(float range) {
    _windupRange = range;
}

void ESCController::update(float currentRPM) {
    // If motor is supposed to be off, ensure it stays off
    if (_targetRPM <= 0) return;

    // If using KV open-loop mode, there's no PID update required here
    if (_controlMode == CONTROL_KV) return;

    unsigned long now = millis();
    unsigned long dtMs = now - _lastPIDTime;
    
    // Run PID calculation at ~20Hz minimum (50ms) to avoid jitter, 
    // but fast enough for control. Let's allow every 10ms.
    if (dtMs < 10) return; 
    
    float dt = dtMs / 1000.0f;
    _lastPIDTime = now;

    // Low Pass Filter
    _filteredRPM = (_filteredRPM * (1.0f - _filterAlpha)) + (currentRPM * _filterAlpha);

    // Debug: Print for Serial Plotter
    // Format: "Label1:Value1,Label2:Value2"
    // Serial.print("Raw:"); Serial.print(currentRPM);
    // Serial.print(",Filtered:"); Serial.println(_filteredRPM);

    float error = _targetRPM - _filteredRPM;
    
    // Integral calculation happens later to check saturation
    
    // Calculate Derivative
    float derivative = (error - _prevError) / dt;
    _prevError = error;
    
    // PID Output
    float output = (_kp * error) + (_ki * _integral) + (_kd * derivative);
    
    // Conditional Integration (Anti-Windup)
    // 1. Check if error is within range
    // 2. Check if output is saturated. If saturated, only integrate if error opposes saturation.
    bool saturated = false;
    int throttle = _minUs + (int)output;
    
    if (throttle > _maxUs) saturated = true;
    if (throttle < _minUs) saturated = true;
    
    bool withinRange = abs(error) < _windupRange;
    
    if (withinRange) {
        if (!saturated) {
             _integral += error * dt;
        } else if (throttle > _maxUs && error < 0) {
             _integral += error * dt; // Desaturate high
        } else if (throttle < _minUs && error > 0) {
             _integral += error * dt; // Desaturate low
        }
    }

    // Map PID output (which is treated as offset) to throttle range
    // Base throttle is _minUs. 
    // Recalculate throttle with updated integral if needed, or just clamp
    output = (_kp * error) + (_ki * _integral) + (_kd * derivative);
    throttle = _minUs + (int)output;
    
    setThrottleMicroseconds(throttle);
}

void ESCController::setControlMode(int mode) {
    if (mode == CONTROL_KV) _controlMode = CONTROL_KV;
    else _controlMode = CONTROL_PID;
}

void ESCController::setMotorKV(float kv) {
    if (kv > 0.0f) _motorKV = kv;
}

void ESCController::setBatteryVoltage(float voltage) {
    if (voltage > 0.0f) _batteryVoltage = voltage;
}

float ESCController::getThrottlePercent() {
    float range = (float)(_maxUs - _minUs);
    if (range <= 0) return 0.0f;
    float pct = ((float)(_lastThrottle - _minUs)) / range;
    pct = constrain(pct, 0.0f, 1.0f);
    return pct * 100.0f; // percent 0..100
}

void ESCController::startTuning() {
    _tuneState = TUNE_RAMP;
    _tuneTimer = millis();
    _tuneBaseThrottle = 1300; // Approx 30% throttle for tuning baseline
    _tuneTargetRPM = 0;
    _tuneCycles = 0;
    _tunePeriodSum = 0;
    setThrottleMicroseconds(_tuneBaseThrottle);
}

bool ESCController::updateTuning(float currentRPM) {
    unsigned long now = millis();
    
    switch (_tuneState) {
        case TUNE_IDLE:
            return false;
            
        case TUNE_RAMP:
            // Wait for motor to spin up
            if (now - _tuneTimer > 3000) {
                _tuneState = TUNE_STABILIZE;
                _tuneTimer = now;
                _tuneTargetRPM = currentRPM; // Capture baseline RPM
            }
            break;
            
        case TUNE_STABILIZE:
            // Wait for stability
            if (now - _tuneTimer > 2000) {
                _tuneState = TUNE_RELAY;
                _tuneLastCrossTime = now;
                _tuneCycles = 0;
            }
            break;
            
        case TUNE_RELAY: {
            // Relay Step: +/- 50us
            int step = 50;
            int high = _tuneBaseThrottle + step;
            int low = _tuneBaseThrottle - step;
            
            if (currentRPM < _tuneTargetRPM) {
                setThrottleMicroseconds(high);
                // Check for crossing upwards
            } else {
                setThrottleMicroseconds(low);
                // Check for crossing downwards
            }
            
            // Simple period detection: count time between rising crossings
            // This is a simplified implementation. Real relay tuning detects peaks.
            // Here we just toggle.
            static bool wasBelow = true;
            bool isBelow = (currentRPM < _tuneTargetRPM);
            
            if (wasBelow && !isBelow) { // Rising edge
                unsigned long period = now - _tuneLastCrossTime;
                _tuneLastCrossTime = now;
                if (_tuneCycles > 0) { // Skip first partial cycle
                    _tunePeriodSum += period;
                }
                _tuneCycles++;
            }
            wasBelow = isBelow;
            
            if (_tuneCycles >= 5) {
                _tuneState = TUNE_CALC;
            }
            break;
        }
        case TUNE_CALC:
            return true;
    }
    return false;
}

PIDConstants ESCController::getTunedPID() {
    // Ziegler-Nichols based on Relay
    // Average Period Tu
    float Tu = (_tunePeriodSum / 4.0f) / 1000.0f; // Seconds
    
    // Amplitude of input oscillation (d) = 50us / 1000us range = 0.05
    // We approximate Ku based on system response. 
    // For a real relay, we need output amplitude. 
    // Let's assume a safe conservative tuning based on period.
    
    // Simplified heuristic for this specific motor type if pure math fails:
    float Ku = 0.5f; // Conservative gain
    
    PIDConstants pid;
    pid.kp = 0.6f * Ku;
    pid.ki = (2.0f * pid.kp) / Tu;
    pid.kd = (pid.kp * Tu) / 8.0f;
    
    stopMotor();
    _tuneState = TUNE_IDLE;
    return pid;
}