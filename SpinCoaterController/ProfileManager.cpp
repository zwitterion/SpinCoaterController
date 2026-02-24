#include "ProfileManager.h"

ProfileManager::ProfileManager(EEPROMStorage& storage) : _storage(storage) {
}

void ProfileManager::begin() {
    // No specific initialization needed
}

bool ProfileManager::createProfile(const SpinProfile& profile) {
    SpinProfile p = profile;
    
    // If ID is 255 (sent by UI for new profiles), find the first available slot
    if (p.id == 255) {
        bool found = false;
        // Try IDs 0 to 9 (matching EEPROMStorage limit)
        for (uint8_t i = 0; i < 10; i++) {
            SpinProfile temp;
            // loadProfile returns false if slot is empty or invalid
            if (!_storage.loadProfile(i, temp)) {
                p.id = i;
                // Try to save to verify it's a valid slot
                if (_storage.saveProfile(p)) {
                    found = true;
                    break;
                }
            }
        }
        return found;
    }
    
    // If ID is specified, just try to save
    return _storage.saveProfile(p);
}

bool ProfileManager::updateProfile(const SpinProfile& profile) {
    return _storage.saveProfile(profile);
}

bool ProfileManager::deleteProfile(uint8_t id) {
    return _storage.deleteProfile(id);
}

bool ProfileManager::getProfile(uint8_t id, SpinProfile& profile) {
    return _storage.loadProfile(id, profile);
}

void ProfileManager::getProfileList(JsonDocument& doc) {
    _storage.listProfiles(doc);
}