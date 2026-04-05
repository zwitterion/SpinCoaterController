#include <Arduino.h>
#include <EEPROM.h>
#include "types.h"
#include "EEPROMStorage.h"
#include "WiFiManager.h"
#include "ProfileManager.h"
#include "RPMReader.h"
#include "ESCController.h"
#include "SafetyManager.h"
#include "ExecutionEngine.h"
#include "WebServer.h"
#include "ButtonHandler.h"
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define PIN_ESC 9
#define PIN_RPM 8
#define PIN_RPM_LED 7  // Moved from 6 to 7 to free up D6 for Stop button
#define PIN_BTN_START 5
#define PIN_BTN_STOP 4

// ============================================================
// GLOBAL VARIABLES
// ============================================================
bool g_rpmCheckEnabled = true; // Global flag for RPM safety checks

// ============================================================
// GLOBAL OBJECTS
// ============================================================

EEPROMStorage storage;
WiFiManager wifiManager(storage);
ProfileManager profileManager(storage);
RPMReader rpmReader(PIN_RPM, PIN_RPM_LED);
ESCController escController(PIN_ESC);
SafetyManager safetyManager(escController);
ButtonHandler buttonHandler(PIN_BTN_START, PIN_BTN_STOP);
ExecutionEngine engine(rpmReader, escController, safetyManager, profileManager, storage);
WiFiUDP udp;
MDNS mdns(udp);
WebServer webServer(80, profileManager, engine, storage, wifiManager);

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    // Wait for serial for debugging (optional, remove for production if needed)
    // while(!Serial) delay(10);
    
    Serial.println(F("SpinCoater Booting 1..."));

    // 1. Initialize Storage
    storage.begin();

    
    // Load RPM Check setting (stored at 510, just before profiles)
    EEPROM.get(510, g_rpmCheckEnabled);
    if (g_rpmCheckEnabled != 0 && g_rpmCheckEnabled != 1) g_rpmCheckEnabled = true; // Sanity check

    // 2. Initialize Hardware
    Serial.println(F("Initializing Hardware..."));

    buttonHandler.begin();
    escController.begin(); // Ensures motor is stopped 
    rpmReader.begin();
    safetyManager.begin();
    
    // 3. Load Settings & Apply
    Serial.println(F("Loading Settings..."));

    SystemSettings settings;
    storage.loadSettings(settings);
    Serial.println(F("Settings Loaded. Applying to ESC..."));
    escController.setPID(settings.pid.kp, settings.pid.ki, settings.pid.kd);
    escController.setCalibration(settings.escMinMicros, settings.escMaxMicros);
    escController.setFilterAlpha(settings.filterAlpha);
    escController.setWindupRange(settings.windupRange);
    
    // If checks are disabled, force KV mode, otherwise use stored setting
    if (!g_rpmCheckEnabled) {
        escController.setControlMode(CONTROL_KV);
    } else {
        escController.setControlMode((int)settings.controlMode);
    }
    
    // 3.1 Apply empirical mapping parameters
    escController.setMappingParams(settings.mapSlope, settings.mapStartPWM);
    
    safetyManager.setMaxRPM(settings.maxRPM);
    
    // 4. Initialize Logic
    profileManager.begin();
    engine.begin();
    
    // 5. Initialize Network
    wifiManager.begin();
    webServer.begin();
    Serial.println(F("Network Ready."));

    // Start Multicast DNS (mDNS) responder
    IPAddress localIP = WiFi.localIP();
    if (localIP != IPAddress(0,0,0,0)) {
        Serial.print(F("Starting mDNS: ")); Serial.println(settings.wifi.hostname);
        mdns.begin(localIP, settings.wifi.hostname);
    }
    
    Serial.println(F("System Ready."));
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
    // Update Button states
    TelemetryData tel = engine.getTelemetry();
    tel.btnStartPressed = buttonHandler.isStartPressed();
    tel.btnStopPressed = buttonHandler.isStopPressed();

    // Physical button logic: Stop the motor/profile if Stop button is pressed
    if (tel.btnStopPressed) {
        engine.stop();
    }

    // 1. Critical Control Loop
    engine.update();
    
    // 2. Network Handling
    wifiManager.update();
    webServer.update();
    
    // Keep mDNS active
    mdns.run();

    // 3. Telemetry Broadcast (Throttle this to ~10Hz to save bandwidth)
    static unsigned long lastTelemetry = 0;
    if (millis() - lastTelemetry > 100) {
        lastTelemetry = millis();
        webServer.broadcastTelemetry(tel);
    }
}