#include "SafetyManager.h"

extern bool g_rpmCheckEnabled;

SafetyManager::SafetyManager(ESCController& esc) : _esc(esc), _lastError(nullptr), _zeroRPMTimer(0), _errorState(false), _maxRPM(15000) {
}

void SafetyManager::begin() {
    clearError();
}

void SafetyManager::setMaxRPM(int maxRPM) {
    _maxRPM = maxRPM;
}

bool SafetyManager::check(float currentRPM, float targetRPM, float throttleOutput) {
    // If RPM checks are disabled by user, bypass all safety logic
    if (!g_rpmCheckEnabled) return true;

    // If already in error state, stay there
    if (_errorState) return false;

    // 1. Stall Detection (No RPM feedback)
    // If throttle is active (>1100us) but RPM is very low for a duration
    const int MIN_ACTIVE_THROTTLE = 1100;
    const float MIN_DETECTABLE_RPM = 100.0;
    const unsigned long STALL_TIMEOUT_MS = 2000;

    if (throttleOutput > MIN_ACTIVE_THROTTLE && currentRPM < MIN_DETECTABLE_RPM) {
        if (_zeroRPMTimer == 0) {
            _zeroRPMTimer = millis();
        } else if (millis() - _zeroRPMTimer > STALL_TIMEOUT_MS) {
            emergencyStop("Stall Detected: Motor not spinning");
            return false;
        }
    } else {
        _zeroRPMTimer = 0;
    }

    // 2. Overspeed Protection
    // If RPM exceeds target by >15% (and target is not zero/low)
    if (targetRPM > 500 && currentRPM > (targetRPM * 2.15)) {  // 1.15
        // Dsiplay the actual RPM and target for debugging
        Serial.print("Overspeed: Current RPM = ");Serial.print(currentRPM);
        Serial.print(", Target RPM = ");Serial.println(targetRPM);
        emergencyStop("Overspeed: RPM > Target + 15%" );
        
        return false;
    }
    
    // 3. Absolute Maximum RPM Safety
    // Hard limit for the DYS D2830 850KV on typical voltage (e.g. 12V -> ~10k RPM)
    if (currentRPM > _maxRPM) { 
        emergencyStop("Critical: RPM Limit Exceeded");
        return false;
    }

    return true;
}

void SafetyManager::emergencyStop(const char* reason) {
    if (!_errorState) {
        _errorState = true;
        _lastError = reason;
        _esc.stopMotor();
        Serial.print("EMERGENCY STOP TRIGGERED: ");
        Serial.println(reason);
    }
}

const char* SafetyManager::getLastError() {
    return _lastError;
}

void SafetyManager::clearError() {
    _errorState = false;
    _lastError = nullptr;
    _zeroRPMTimer = 0;
}