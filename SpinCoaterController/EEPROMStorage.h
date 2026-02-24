#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "types.h"

class EEPROMStorage {
public:
    EEPROMStorage();
    
    void begin();
    
    // Settings
    void loadSettings(SystemSettings& settings);
    void saveSettings(const SystemSettings& settings);
    
    // Profiles
    // We store profiles in a fixed area. 
    // Returns true if successful.
    bool saveProfile(const SpinProfile& profile);
    bool loadProfile(uint8_t id, SpinProfile& profile);
    bool deleteProfile(uint8_t id);
    
    // Helper to list available profile IDs/Names
    void listProfiles(JsonDocument& doc);
    void wipe(); // Factory reset

private:
    uint16_t calculateCRC(const uint8_t* data, size_t length);
};

#endif