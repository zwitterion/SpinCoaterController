#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include "types.h"
#include "ESCController.h"

class SafetyManager {
public:
    SafetyManager(ESCController& esc);
    
    void begin();
    void setMaxRPM(int maxRPM);
    
    // Returns true if system is safe, false if emergency stop triggered
    bool check(float currentRPM, float targetRPM, float throttleOutput);
    
    void emergencyStop(const char* reason);
    
    const char* getLastError();
    void clearError();

private:
    ESCController& _esc;
    const char* _lastError;
    unsigned long _zeroRPMTimer;
    bool _errorState;
    int _maxRPM;
};

#endif