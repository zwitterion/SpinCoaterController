#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================
// ENUMS
// ============================================================

enum SystemState {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_STOPPING,
    STATE_ERROR,
    STATE_EMERGENCY_STOP,
    STATE_TUNING,
    STATE_MANUAL,
    STATE_CALIBRATING,
    STATE_MAPPING
};

enum RampType {
    RAMP_LINEAR = 0,
    RAMP_EXPONENTIAL = 1,
    RAMP_S_CURVE = 2
};

enum ControlMode {
    CONTROL_PID = 0,
    CONTROL_KV = 1
};

// ============================================================
// DATA STRUCTURES
// ============================================================

struct PIDConstants {
    float kp;
    float ki;
    float kd;
};

struct SpinStep {
    int startRPM;
    int targetRPM;
    unsigned long rampDurationMs;
    unsigned long holdDurationMs;
    RampType rampType;
};

struct SpinProfile {
    char name[21]; // Max 20 chars + null terminator
    uint8_t stepCount;
    SpinStep steps[20]; // Max 20 steps
    uint8_t id; // Internal ID for storage
};

struct WiFiCredentials {
    char ssid[33];
    char password[65];
    bool valid;
};

struct SystemSettings {
    PIDConstants pid;
    int minRPM;
    int maxRPM;
    int escMinMicros; // Usually 1000
    int escMaxMicros; // Usually 2000
    WiFiCredentials wifi;
    float filterAlpha; // 0.0 to 1.0 (1.0 = no filter)
    float windupRange; // RPM error range for integration
    bool escCalibrated;
    ControlMode controlMode;
    // Mapping Results
    float mapSlope;
    float mapIntercept;
    int mapStartPWM;
};

// ============================================================
// TELEMETRY
// ============================================================

struct TelemetryData {
    SystemState state;
    float currentRPM;
    float targetRPM;
    float throttlePercent;
    int pulseWidth;
    int currentStepIndex;
    unsigned long stepTimeRemaining;
    const char* errorMessage;
    int profileId;
    bool isMapPoint;
};

#endif