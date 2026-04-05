#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(int pinStart, int pinStop) 
    : _pinStart(pinStart), _pinStop(pinStop) {
}

void ButtonHandler::begin() {
    pinMode(_pinStart, INPUT_PULLUP);
    pinMode(_pinStop, INPUT_PULLUP);
}

bool ButtonHandler::isStartPressed() {
    return digitalRead(_pinStart) == LOW;
}

bool ButtonHandler::isStopPressed() {
    return digitalRead(_pinStop) == LOW;
}