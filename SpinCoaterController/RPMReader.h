
#ifndef RPM_READER_H
#define RPM_READER_H

#include <Arduino.h>

class RPMReader {
public:
    RPMReader(uint8_t pin, uint8_t pinLED);
    
    void begin();
    
    // Returns the current RPM, handling timeouts
    float getRPM();
    
    // Interrupt Service Routine handler
    void handleInterrupt();

private:
    uint8_t _pin;
    uint8_t _pinLED;
    volatile unsigned long _lastPulseTime;
    volatile unsigned long _pulseInterval;
    volatile bool _newData;
    // Small buffer for median filtering of recent intervals (in microseconds)
    unsigned long _intervalBuf[3];
    uint8_t _intervalIdx;
    
    unsigned long _lastRPMCalculationTime;
    float _currentRPM;
};

#endif