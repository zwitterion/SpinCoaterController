#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFiS3.h>
#include "Arduino_LED_Matrix.h"
#include "types.h"
#include "EEPROMStorage.h"

class WiFiManager {
public:
    WiFiManager(EEPROMStorage& storage);
    
    void begin();
    void update();
    
    bool isAPMode() const;
    String getIPAddress();
    
    // Used to save new credentials from the Web UI
    void saveCredentials(const char* ssid, const char* password);

private:
    EEPROMStorage& _storage;
    ArduinoLEDMatrix _matrix;
    bool _isAP;
    unsigned long _lastMatrixUpdate;
    
    void startAP();
    bool connectToStation();
    
    // LED Matrix helpers
    void displayIPScroll();
    void displayError();
    void displayWifiIcon();
    void displayAPIcon();
};

#endif