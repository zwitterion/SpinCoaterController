#ifndef EXECUTION_ENGINE_H
#define EXECUTION_ENGINE_H

#include "types.h"
#include "RPMReader.h"
#include "ESCController.h"
#include "SafetyManager.h"
#include "ProfileManager.h"
#include "EEPROMStorage.h"

class ExecutionEngine {
public:
    ExecutionEngine(RPMReader& rpm, ESCController& esc, SafetyManager& safety, ProfileManager& pm, EEPROMStorage& storage);
    
    void begin();
    void update();
    
    // Commands
    void runProfile(uint8_t profileId);
    void stop();
    void pause();
    void resume();
    void startTuning();
    void startManual();
    void setManualRPM(float rpm);
    void setCalibrationThrottle(int us);
    
    // Status
    TelemetryData getTelemetry();

private:
    RPMReader& _rpmReader;
    ESCController& _escController;
    SafetyManager& _safetyManager;
    ProfileManager& _profileManager;
    EEPROMStorage& _storage;
    
    SystemState _state;
    SpinProfile _currentProfile;
    
    int _currentStepIndex;
    unsigned long _stepStartTime;
    unsigned long _pauseStartTime;
    
    float _currentTargetRPM;
    float _manualTargetRPM;
    
    // Helper to calculate instantaneous target based on ramp
    float calculateRampRPM(const SpinStep& step, unsigned long elapsedStepTime);
    
    // Transition helpers
    void advanceStep();
};

#endif