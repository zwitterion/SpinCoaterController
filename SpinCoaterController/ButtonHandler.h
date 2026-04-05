#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

class ButtonHandler {
public:
    ButtonHandler(int pinStart, int pinStop);
    
    void begin();
    
    // Returns true if button is currently pressed (Low)
    bool isStartPressed();
    bool isStopPressed();

private:
    int _pinStart;
    int _pinStop;
};

#endif