#include "RPMReader.h"

// Global pointer to instance for ISR
static RPMReader* _rpmReaderInstance = nullptr;

// Static ISR wrapper
void _rpmReaderISR() {
    if (_rpmReaderInstance) {
        _rpmReaderInstance->handleInterrupt();
    }
}

RPMReader::RPMReader(uint8_t pin, uint8_t pinLED) : _pin(pin), _pinLED(pinLED), _lastPulseTime(0), 
    _pulseInterval(0), _newData(false), _lastRPMCalculationTime(0), _currentRPM(0) {
    _rpmReaderInstance = this;
    _intervalBuf[0] = _intervalBuf[1] = _intervalBuf[2] = 0;
    _intervalIdx = 0;
}

void RPMReader::begin() {
    pinMode(_pin, INPUT_PULLUP); // TCRT5000 usually requires pullup if open-collector
    pinMode(_pinLED, OUTPUT);
    digitalWrite(_pinLED, HIGH); // Turn on LED for RPM sensor
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

            // Push into small buffer for median filtering to reject single-sample spikes
            // _intervalBuf[_intervalIdx % 3] = interval;
            // _intervalIdx = (_intervalIdx + 1) % 3;

            // // Compute median of three values
            // unsigned long a = _intervalBuf[0];
            // unsigned long b = _intervalBuf[1];
            // unsigned long c = _intervalBuf[2];
            // unsigned long median;

            // // simple median of three
            // if ((a <= b && b <= c) || (c <= b && b <= a)) median = b;
            // else if ((b <= a && a <= c) || (c <= a && a <= b)) median = a;
            // else median = c;

            // if (median > 0) {
            //     _currentRPM = 60000000.0f / (float)median;
            // }

        }
    }
    
    return _currentRPM;
}