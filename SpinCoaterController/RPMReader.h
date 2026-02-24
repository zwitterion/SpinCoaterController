#ifndef RPM_READER_H
#define RPM_READER_H

#include <Arduino.h>

class RPMReader {
public:
    RPMReader(uint8_t pin);
    
    void begin();
    
    // Returns the current RPM, handling timeouts
    float getRPM();
    
    // Interrupt Service Routine handler
    void handleInterrupt();

private:
    uint8_t _pin;
    volatile unsigned long _lastPulseTime;
    volatile unsigned long _pulseInterval;
    volatile bool _newData;
    
    unsigned long _lastRPMCalculationTime;
    float _currentRPM;
};

#endif