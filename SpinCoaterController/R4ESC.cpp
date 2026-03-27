#include "R4ESC.h"

R4ESC::R4ESC(int pin) : escPwm(pin) {
    _minUs = 1000;
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
	escPwm.begin(50.0f, duty); // 50 Hz
    //escPwm.begin(100.0f, duty); // 100Hz
	delay(3000);
}


void R4ESC::attach(int _pin, int _minUs, int _maxUs) {
    this->_minUs = _minUs;
    this->_maxUs = _maxUs;
	float duty = ((float)_minUs / period_us) * 100.0f;
	escPwm.begin(100.0f, duty);
	delay(3000);
}

void R4ESC::writeMicroseconds(int us) {
	if (us < _minUs) us = _minUs;
	if (us > _maxUs) us = _maxUs;


	float duty = ((float)us / period_us) * 100.0f;
	escPwm.pulse_perc(duty);

    // Serial.print("R4ESC: Set pulse width = ");
    // Serial.print(us);
    // Serial.print(" us (duty = "); Serial.print(duty);
    // Serial.print("%)");
    // Serial.print(", Range = ["); Serial.print(_minUs); Serial.print(".."); Serial.print(_maxUs); Serial.println("]");
    // // write pin #
    // Serial.print(" on pin "); Serial.println(_pin);
    // Serial.println();


}

void R4ESC::writeThrottlePercent(float percent) {
	if (percent < 0.0f) percent = 0.0f;
	if (percent > 100.0f) percent = 100.0f;

	int us = 1000 + (int)(percent * 10.0f);
	writeMicroseconds(us);
}

