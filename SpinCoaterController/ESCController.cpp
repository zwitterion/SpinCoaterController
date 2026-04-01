#include "ESCController.h"

ESCController::ESCController(uint8_t pin) : _pin(pin), _esc(pin), _targetRPM(0), 
    _kp(0), _ki(0), _kd(0), _integral(0), _prevError(0), _lastPIDTime(0),
    _minUs(0), _maxUs(2000), _filterAlpha(1.0), _windupRange(500), _filteredRPM(0),
    _tuneState(TUNE_IDLE), _lastThrottle(0), _controlMode(CONTROL_PID),
    _mapSlope(0.0f), _mapStartPWM(0) {
}

void ESCController::begin() {
    _esc.begin(_minUs); // Arm/init ESC using R4ESC API
    stopMotor(); // Send min throttle to arm ESC
}

void ESCController::setThrottleMicroseconds(int us) {
    // Safety clamp
    us = constrain(us, 0, 2000);
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
            int us = 0;
            
            if (_mapSlope > 0.001f) {
                // Use Empirical Mapping from PWM tuning process
                us = _mapStartPWM + (int)(rpm / _mapSlope);
            } else {
                // If no mapping exists, default to neutral
                us = 0;
            }
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
    setThrottleMicroseconds(0);
}

void ESCController::setPID(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void ESCController::setCalibration(int minUs, int maxUs) {
    _minUs = minUs;
    _maxUs = maxUs;
    _esc.attach(_pin, _minUs, _maxUs); // Propagate limits to hardware wrapper
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

    // currentRPM is now pre-filtered by the RPMReader EMA algorithm
    float error = _targetRPM - currentRPM;
    
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

void ESCController::setMappingParams(float slope, int startPWM) {
    _mapSlope = slope;
    _mapStartPWM = startPWM;
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
    
    // Target 3000 RPM for the tune. Use mapping if available, else a safe default.
    _tuneTargetRPM = 3000.0f;
    if (_mapSlope > 0.001f) {
        _tuneBaseThrottle = _mapStartPWM + (int)(_tuneTargetRPM / _mapSlope);
    } else {
        _tuneBaseThrottle = 1600; // Generic safe forward throttle
    }

    _tuneCycles = 0;
    _tunePeriodSum = 0;
    _tuneAmplitudeSum = 0;
    _tuneMaxRPM = 0;
    _tuneMinRPM = 100000;
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
            // Relay Step: +/- 40us for distinct oscillation
            int step = 50;
            int high = _tuneBaseThrottle + step;
            int low = _tuneBaseThrottle - step;
            
            // Track Peaks
            if (currentRPM > _tuneMaxRPM) _tuneMaxRPM = currentRPM;
            if (currentRPM < _tuneMinRPM) _tuneMinRPM = currentRPM;

            if (currentRPM < _tuneTargetRPM) {
                setThrottleMicroseconds(high);
            } else {
                setThrottleMicroseconds(low);
            }
            
            static bool wasBelow = true;
            bool isBelow = (currentRPM < _tuneTargetRPM);
            
            if (wasBelow && !isBelow) { // Rising edge
                unsigned long period = now - _tuneLastCrossTime;
                _tuneLastCrossTime = now;

                if (_tuneCycles > 0) { 
                    _tunePeriodSum += period;
                    // Amplitude a = (max - min) / 2
                    float amplitude = (_tuneMaxRPM - _tuneMinRPM) / 2.0f;
                    _tuneAmplitudeSum += amplitude;
                }
                
                // Reset peaks for next cycle
                _tuneMaxRPM = _tuneTargetRPM;
                _tuneMinRPM = _tuneTargetRPM;
                _tuneCycles++;
            }
            wasBelow = isBelow;
            
            if (_tuneCycles >= 6) { // 5 full cycles
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
    // Ziegler-Nichols Relay Method
    // d = relay step (50), a = amplitude of oscillation
    float avgTu = (_tunePeriodSum / 5.0f) / 1000.0f; // Average period in seconds
    float avgA = _tuneAmplitudeSum / 5.0f;          // Average RPM amplitude

    if (avgA < 1.0f) avgA = 1.0f; // Prevent div by zero

    // Ultimate Gain Ku = (4 * d) / (pi * a)
    // We use the relay step (50us) as 'd'
    float Ku = (4.0f * 50.0f) / (PI * avgA);
    
    PIDConstants pid;
    // Standard Ziegler-Nichols PID tuning parameters
    pid.kp = 0.60f * Ku;
    pid.ki = 1.20f * Ku / avgTu;
    pid.kd = 0.075f * Ku * avgTu;
    
    stopMotor();
    _tuneState = TUNE_IDLE;
    return pid;
}