#include "WiFiManager.h"

// Simple 8x12 bitmap for WiFi icon
const uint32_t icon_wifi[] = {
    0x00000000,
    0x00066000,
    0x00999900,
    0x08000100,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
};

// Simple AP icon (A P)
const uint32_t icon_ap[] = {
    0x300f0000,
    0x50090000,
    0x700f0000,
    0x50080000,
    0x50080000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
};

WiFiManager::WiFiManager(EEPROMStorage& storage) : _storage(storage), _isAP(false), _lastMatrixUpdate(0) {
}

void WiFiManager::begin() {
    _matrix.begin();
    
    SystemSettings settings;
    _storage.loadSettings(settings);
    
    bool connected = false;
    
    if (settings.wifi.valid) {
        Serial.print("Connecting to: ");
        Serial.println(settings.wifi.ssid);
        
        // Try to connect
        WiFi.begin(settings.wifi.ssid, settings.wifi.password);
        
        // Wait up to 10 seconds
        for (int i = 0; i < 20; i++) {
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                break;
            }
            delay(500);
            Serial.print(".");
        }
        Serial.println();
    }
    
    if (connected) {
        Serial.println("WiFi Connected!");
        Serial.print("IP: ");
        unsigned long startWait = millis();
        while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - startWait < 5000) {
            delay(100);
        }
        Serial.println(WiFi.localIP());
        _isAP = false;
        displayWifiIcon();
    } else {
        startAP();
    }
}

void WiFiManager::startAP() {
    Serial.println("Starting AP Mode...");
    
    // Create unique SSID based on MAC
    byte mac[6];
    WiFi.macAddress(mac);
    char ssid[32];
    snprintf(ssid, 32, "SpinCoater-%02X%02X", mac[4], mac[5]);
    
    WiFi.beginAP(ssid);
    
    Serial.print("AP Started: ");
    Serial.println(ssid);
    Serial.print("IP: ");
    unsigned long startWait = millis();
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - startWait < 5000) {
        delay(100);
    }
    Serial.println(WiFi.localIP());
    
    _isAP = true;
    displayAPIcon();
}

void WiFiManager::update() {
    // Periodically check connection status if not in AP mode
    if (!_isAP && WiFi.status() != WL_CONNECTED) {
        Serial.println("Connection lost! Reconnecting...");
        WiFi.disconnect();
        begin(); // Retry connection logic
    }
    
    // In AP mode, scroll IP every 5 seconds
    if (_isAP && millis() - _lastMatrixUpdate > 5000) {
        _lastMatrixUpdate = millis();
        displayIPScroll();
    }
}

bool WiFiManager::isAPMode() const {
    return _isAP;
}

String WiFiManager::getIPAddress() {
    return _isAP ? WiFi.localIP().toString() : WiFi.localIP().toString();
}

void WiFiManager::saveCredentials(const char* ssid, const char* password) {
    SystemSettings settings;
    _storage.loadSettings(settings);
    
    strncpy(settings.wifi.ssid, ssid, 32);
    strncpy(settings.wifi.password, password, 64);
    settings.wifi.valid = true;
    
    _storage.saveSettings(settings);
    // Reboot is usually required or handled by caller
}

void WiFiManager::displayWifiIcon() {
    _matrix.loadFrame(icon_wifi);
}

void WiFiManager::displayAPIcon() {
    _matrix.loadFrame(icon_ap);
}

void WiFiManager::displayIPScroll() {
    // The R4 LED Matrix library handles scrolling text via drawString usually,
    // but for simplicity in this modular setup, we'll just print to Serial
    // or implement a basic scroller if the library version supports it.
    // For now, we re-show the AP icon to indicate state.
    displayAPIcon();
}

void WiFiManager::displayError() {
    // X icon
    const uint32_t icon_error[] = {
        0x81042000,
        0x42024000,
        0x24018000,
        0x18018000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000
    };
    _matrix.loadFrame(icon_error);
}