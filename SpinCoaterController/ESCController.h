#ifndef ESC_CONTROLLER_H
#define ESC_CONTROLLER_H

#include <Arduino.h>
#include <Servo.h>
#include "types.h"

class ESCController {
public:
    ESCController(uint8_t pin);
    
    void begin();
    
    // Direct throttle control (1000-2000us)
    void setThrottleMicroseconds(int us);
    int getThrottleMicroseconds();
    
    // Closed loop control
    void setTargetRPM(float rpm);
    void update(float currentRPM); // Call this frequently
    
    void stopMotor();
    
    void setPID(float kp, float ki, float kd);
    void setCalibration(int minUs, int maxUs);
    void setFilterAlpha(float alpha);
    void setWindupRange(float range);
    
    // Tuning
    void startTuning();
    bool updateTuning(float currentRPM); // Returns true when complete
    PIDConstants getTunedPID();

private:
    uint8_t _pin;
    Servo _esc;
    
    float _targetRPM;
    float _kp, _ki, _kd;
    float _integral;
    float _prevError;
    unsigned long _lastPIDTime;
    
    int _minUs, _maxUs;
    
    // Advanced PID
    float _filterAlpha;
    float _windupRange;
    float _filteredRPM;
    
    // Tuning State
    enum TuningState { TUNE_IDLE, TUNE_RAMP, TUNE_STABILIZE, TUNE_RELAY, TUNE_CALC };
    TuningState _tuneState;
    unsigned long _tuneTimer;
    int _tuneBaseThrottle;
    float _tuneTargetRPM;
    int _tuneCycles;
    unsigned long _tuneLastCrossTime;
    float _tunePeriodSum;
    int _lastThrottle;
};

#endif