#ifndef R4ESC_H
#define R4ESC_H

#include <Arduino.h>
#include "pwm.h"

class R4ESC {
  private:
    PwmOut escPwm;
    const float period_us = 20000.0f; // 50 Hz = 20ms = 20,000µs
    //const float period_us = 10000.0f; // 100 Hz = 10ms = 10,000µs
    int _minUs, _maxUs;
    int _pin;


  public:
    // Initialize the hardware PWM on the specified pin
    R4ESC(int pin);

    // Arms the ESC (usually requires holding the lowest throttle for a few seconds)
    void begin(int armPulse_us = 1000);
    void attach(int _pin, int _minUs, int _maxUs);

    // Set the throttle based on pulse width in microseconds (typically 1000 to 2000)
    void writeMicroseconds(int us);
    
    // Optional helper: Set throttle by percentage (0% to 100%)
    void writeThrottlePercent(float percent);
};

#endif