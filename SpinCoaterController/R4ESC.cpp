#include "R4ESC.h"

R4ESC::R4ESC(int pin) : escPwm(pin) {
    _minUs = 0;
    _maxUs = 2000;
    _pin = pin;

    // (High-Drive mode can cause issues with some ESCs)
    // this might help?
    pinMode(_pin, OUTPUT);
    // for pin 9
    //R_PFS->PORT[3].PIN[4].PmnPFS_b.PDR = 0;

}

void R4ESC::begin(int armPulse_us) {
	float duty = ((float)armPulse_us / period_us) * 100.0f;
	//period 50us = 20000hz; pulse 0 us = 0%
    escPwm.begin(frequency_hz, duty); 
    
	delay(100);
}


void R4ESC::attach(int _pin, int _minUs, int _maxUs) {
    this->_minUs = _minUs;
    this->_maxUs = _maxUs;
    // Do not call escPwm.begin again as it is already initialized in constructor.
    // Simply update the internal limits.
    writeMicroseconds(_minUs);
}

void R4ESC::writeMicroseconds(int us) {
	if (us < _minUs) us = _minUs;
	if (us > _maxUs) us = _maxUs;


	if (_maxUs <= _minUs) {
        // Avoid division by zero, set to 0% duty
        escPwm.pulse_perc(0);
        return;
    }
    float duty = ((float)us / (_maxUs - _minUs)) * 100.0f;
	escPwm.pulse_perc(duty);

    // Serial.print("R4ESC: Set pulse width = ");
    // Serial.print(us);
    // Serial.print(" us (duty = "); Serial.print(duty);
    // Serial.print("%)");
    // Serial.print(", Range = ["); Serial.print(_minUs); Serial.print(".."); Serial.print(_maxUs); Serial.println("]");
    // // write pin #
    // Serial.print(" on pin "); Serial.println(_pin);
    // Serial.print(" period: "); Serial.println(period_us);
    // Serial.println();


}

void R4ESC::writeThrottlePercent(float percent) {
	if (percent < 0.0f) percent = 0.0f;
	if (percent > 100.0f) percent = 100.0f;

	float range = (float)(_maxUs - _minUs);
	int us = _minUs + (int)(percent * (range / 100.0f));
	writeMicroseconds(us);
}
