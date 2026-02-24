#include <Arduino.h>
#include "types.h"
#include "EEPROMStorage.h"
#include "WiFiManager.h"
#include "ProfileManager.h"
#include "RPMReader.h"
#include "ESCController.h"
#include "SafetyManager.h"
#include "ExecutionEngine.h"
#include "WebServer.h"

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define PIN_ESC 9
#define PIN_RPM 2

// ============================================================
// GLOBAL OBJECTS
// ============================================================

EEPROMStorage storage;
WiFiManager wifiManager(storage);
ProfileManager profileManager(storage);
RPMReader rpmReader(PIN_RPM);
ESCController escController(PIN_ESC);
SafetyManager safetyManager(escController);
ExecutionEngine engine(rpmReader, escController, safetyManager, profileManager, storage);
WebServer webServer(80, profileManager, engine, storage, wifiManager);

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    // Wait for serial for debugging (optional, remove for production if needed)
    // while(!Serial) delay(10);
    
    Serial.println(F("SpinCoater Booting..."));

    // 1. Initialize Storage
    storage.begin();
    
    // 2. Initialize Hardware
    escController.begin(); // Ensures motor is stopped (1000us)
    rpmReader.begin();
    safetyManager.begin();
    
    // 3. Load Settings & Apply
    SystemSettings settings;
    storage.loadSettings(settings);
    escController.setPID(settings.pid.kp, settings.pid.ki, settings.pid.kd);
    escController.setCalibration(settings.escMinMicros, settings.escMaxMicros);
    escController.setFilterAlpha(settings.filterAlpha);
    escController.setWindupRange(settings.windupRange);
    safetyManager.setMaxRPM(settings.maxRPM);
    
    // 4. Initialize Logic
    profileManager.begin();
    engine.begin();
    
    // 5. Initialize Network
    wifiManager.begin();
    webServer.begin();
    
    Serial.println(F("System Ready."));
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
    // 1. Critical Control Loop
    engine.update();
    
    // 2. Network Handling
    wifiManager.update();
    webServer.update();
    
    // 3. Telemetry Broadcast (Throttle this to ~10Hz to save bandwidth)
    static unsigned long lastTelemetry = 0;
    if (millis() - lastTelemetry > 100) {
        lastTelemetry = millis();
        webServer.broadcastTelemetry(engine.getTelemetry());
    }
}