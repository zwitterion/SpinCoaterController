#include "RPMReader.h"

// Global pointer to instance for ISR
static RPMReader* _rpmReaderInstance = nullptr;

// Static ISR wrapper
void _rpmReaderISR() {
    if (_rpmReaderInstance) {
        _rpmReaderInstance->handleInterrupt();
    }
}

RPMReader::RPMReader(uint8_t pin) : _pin(pin), _lastPulseTime(0), 
    _pulseInterval(0), _newData(false), _lastRPMCalculationTime(0), _currentRPM(0) {
    _rpmReaderInstance = this;
}

void RPMReader::begin() {
    pinMode(_pin, INPUT_PULLUP); // TCRT5000 usually requires pullup if open-collector
    attachInterrupt(digitalPinToInterrupt(_pin), _rpmReaderISR, FALLING);
}

void RPMReader::handleInterrupt() {
    unsigned long now = micros();
    unsigned long diff = now - _lastPulseTime;
    
    // Software Debounce
    // 10,000 RPM = 166Hz = 6000us period.
    // Reject pulses shorter than 500us (corresponds to >120,000 RPM or noise)
    if (diff > 500) {
        _pulseInterval = diff;
        _lastPulseTime = now;
        _newData = true;
    }
}

float RPMReader::getRPM() {
    unsigned long now = micros();
    
    // Timeout Logic:
    // If no pulse for 0.5 seconds, assume stopped (RPM < 120)
    // We check this first to ensure RPM drops to 0 when motor stops.
    if (now - _lastPulseTime > 500000) {
        _currentRPM = 0;
        return 0.0f;
    }

    // Calculate RPM if new data available
    if (_newData) {
        // Atomic read of volatile variables
        noInterrupts();
        unsigned long interval = _pulseInterval;
        _newData = false;
        interrupts();
        
        if (interval > 0) {
            // RPM = 60 seconds / (interval in seconds)
            // RPM = 60,000,000 us / interval_us
            _currentRPM = 60000000.0f / interval;
        }
    }
    
    return _currentRPM;
}