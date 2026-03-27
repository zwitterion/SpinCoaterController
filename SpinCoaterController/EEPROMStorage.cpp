#include "EEPROMStorage.h"

// Magic number to verify if EEPROM has been initialized before
const uint32_t EEPROM_MAGIC = 0xCAFEBABE;
const int ADDR_MAGIC = 0;
const int ADDR_SETTINGS = sizeof(uint32_t);
// Start profiles after settings. Leaving 512 bytes for settings/future expansion.
const int ADDR_PROFILES_START = 512; 
const int MAX_PROFILES = 10; // Limit based on EEPROM size (R4 has 8KB, profile is ~450b)

EEPROMStorage::EEPROMStorage() {
}

void EEPROMStorage::begin() {
    // EEPROM.begin() is not strictly needed for the standard internal EEPROM lib 
    // on R4, but good practice if porting.
    
    uint32_t magic;
    EEPROM.get(ADDR_MAGIC, magic);
    
    if (magic != EEPROM_MAGIC) {
        wipe();
    }
}

void EEPROMStorage::wipe() {
    // Initialize default settings
    SystemSettings defaults;
    defaults.pid = {0.1f, 0.05f, 0.01f}; // Conservative defaults
    defaults.minRPM = 0;
    defaults.maxRPM = 10000; // Adjust based on motor
    defaults.escMinMicros = 1000;
    defaults.escMaxMicros = 2000;
    defaults.wifi.valid = false;
    memset(defaults.wifi.ssid, 0, 33);
    memset(defaults.wifi.password, 0, 65);
    defaults.filterAlpha = 1.0f; // Default no filtering
    defaults.windupRange = 500.0f; // Default 500 RPM range
    defaults.escCalibrated = false;
    defaults.controlMode = CONTROL_PID;

    saveSettings(defaults);
    
    // Reset RPM Check to default (ON)
    EEPROM.put(510, true);
    
    // Clear profiles
    for (int i = 0; i < MAX_PROFILES; i++) {
        int addr = ADDR_PROFILES_START + (i * sizeof(SpinProfile));
        EEPROM.write(addr, 0); 
    }
    
    // Mark as initialized
    EEPROM.put(ADDR_MAGIC, EEPROM_MAGIC);
}

void EEPROMStorage::loadSettings(SystemSettings& settings) {
    EEPROM.get(ADDR_SETTINGS, settings);
}

void EEPROMStorage::saveSettings(const SystemSettings& settings) {
    EEPROM.put(ADDR_SETTINGS, settings);
}

bool EEPROMStorage::saveProfile(const SpinProfile& profile) {
    if (profile.id >= MAX_PROFILES) return false;
    
    // Ensure we are not saving a "deleted" marker by accident
    if (profile.name[0] == 0 || profile.name[0] == 0xFF) {
        return false;
    }

    int addr = ADDR_PROFILES_START + (profile.id * sizeof(SpinProfile));
    EEPROM.put(addr, profile);
    return true;
}

bool EEPROMStorage::loadProfile(uint8_t id, SpinProfile& profile) {
    if (id >= MAX_PROFILES) return false;
    
    int addr = ADDR_PROFILES_START + (id * sizeof(SpinProfile));
    EEPROM.get(addr, profile);
    
    // Basic validation: check if name is empty or starts with null
    if (profile.name[0] == 0 || profile.name[0] == 0xFF) {
        return false; // Empty slot
    }
    return true;
}

bool EEPROMStorage::deleteProfile(uint8_t id) {
    if (id >= MAX_PROFILES) return false;
    
    int addr = ADDR_PROFILES_START + (id * sizeof(SpinProfile));
    
    // We just null out the first byte of the name to mark as deleted
    EEPROM.write(addr, 0); 
    return true;
}

void EEPROMStorage::listProfiles(JsonDocument& doc) {
    JsonArray arr = doc.to<JsonArray>();
    
    for (int i = 0; i < MAX_PROFILES; i++) {
        int addr = ADDR_PROFILES_START + (i * sizeof(SpinProfile));
        char firstChar = EEPROM.read(addr);
        
        // Check if valid (not deleted/empty)
        if (firstChar != 0 && firstChar != 0xFF) {
            SpinProfile temp;
            EEPROM.get(addr, temp);
            
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = temp.id;
            obj["name"] = temp.name;
        }
    }
}

uint16_t EEPROMStorage::calculateCRC(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}