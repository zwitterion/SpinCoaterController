#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include "types.h"
#include "EEPROMStorage.h"

class ProfileManager {
public:
    ProfileManager(EEPROMStorage& storage);
    
    void begin();
    
    bool createProfile(const SpinProfile& profile);
    bool updateProfile(const SpinProfile& profile);
    bool deleteProfile(uint8_t id);
    bool getProfile(uint8_t id, SpinProfile& profile);
    
    // Fills a JSON document with the list of profiles (ID and Name only)
    void getProfileList(JsonDocument& doc);

private:
    EEPROMStorage& _storage;
    // Optional: Cache metadata in RAM if needed
};

#endif