#ifndef R4ESC_H
#define R4ESC_H

#include <Arduino.h>
#include "pwm.h"

class R4ESC {
  private:
    PwmOut escPwm;


    const float period_us = 50.0; // 50us period = 20,000 Hz frequency
    const float frequency_hz = 1000000.0f / period_us; // 20,000 Hz

    int _minUs, _maxUs;
    int _pin;


  public:
    // Initialize the hardware PWM on the specified pin
    R4ESC(int pin);

    void begin(int armPulse_us = 0);
    void attach(int _pin, int _minUs, int _maxUs);

    // Set the throttle based on pulse width in microseconds (typically 0 to 2000)
    void writeMicroseconds(int us);
    
    // Optional helper: Set throttle by percentage (0% to 100%)
    void writeThrottlePercent(float percent);
};

#endif