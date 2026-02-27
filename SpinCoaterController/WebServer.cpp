#include "WebServer.h"
#include "ESCController.h"
#include <EEPROM.h>

// External reference to allow runtime PID updates
extern ESCController escController;
extern bool g_rpmCheckEnabled;

WebServer::WebServer(int port, ProfileManager& pm, ExecutionEngine& engine, EEPROMStorage& storage, WiFiManager& wifi)
    : _server(port), _pm(pm), _engine(engine), _storage(storage), _wifi(wifi), _wsConnected(false) {
}

void WebServer::begin() {
    _server.begin();
}

void WebServer::update() {
    // 1. Handle New Clients
    WiFiClient newClient = _server.available();
    if (newClient) {
        if (newClient.connected()) {
            // Wait briefly for data to be available to avoid reading empty request
            unsigned long startWait = millis();
            while (newClient.connected() && !newClient.available() && millis() - startWait < 500) {
                delay(1);
            }

            // Read Request Line
            String reqLine = newClient.readStringUntil('\r');
            newClient.read(); // consume \n
            
            // Read Headers
            String headers = "";
            unsigned long timeout = millis();
            while (newClient.connected() && millis() - timeout < 2000) {
                if (newClient.available()) {
                    String line = newClient.readStringUntil('\n');
                    if (line.endsWith("\r")) line.remove(line.length() - 1);
                    if (line.length() == 0) break; // End of headers
                    headers += line + "\n";
                    timeout = millis(); // Reset timeout on data
                }
            }
            
            // Check for WebSocket Upgrade
            if (_wsHandler.isUpgradeRequest(headers)) {
                Serial.println("WS: Upgrade Request Detected");
                if (_wsHandler.handleHandshake(newClient, headers)) {
                    Serial.println("WS: Handshake Success");
                    // Close existing if any (simple single-client support)
                    if (_wsConnected && _wsClient.connected()) _wsClient.stop();
                    
                    _wsClient = newClient;
                    _wsConnected = true;
                } else {
                    Serial.println("WS: Handshake Failed");
                    newClient.stop();
                }
            } else {
                // Handle HTTP Request
                int firstSpace = reqLine.indexOf(' ');
                int secondSpace = reqLine.lastIndexOf(' ');
                
                if (firstSpace > 0 && secondSpace > firstSpace) {
                    String method = reqLine.substring(0, firstSpace);
                    String uri = reqLine.substring(firstSpace + 1, secondSpace);
                    
                    // Parse Content-Length for Body
                    int contentLength = 0;
                    if (method == "POST" || method == "PUT") {
                        int clIndex = headers.indexOf("Content-Length: ");
                        if (clIndex != -1) {
                            int clEnd = headers.indexOf("\n", clIndex);
                            contentLength = headers.substring(clIndex + 16, clEnd).toInt();
                        }
                    }
                    
                    String body = "";
                    if (contentLength > 0) {
                        unsigned long start = millis();
                        while (body.length() < (unsigned int)contentLength && millis() - start < 1000) {
                            if (newClient.available()) {
                                body += (char)newClient.read();
                            }
                        }
                    }
                    
                    handleApiRequest(newClient, method, uri, body);
                }
                newClient.stop();
            }
        }
    }
    
    handleWebSocket();
}

void WebServer::handleWebSocket() {
    if (_wsConnected) {
        if (!_wsClient.connected()) {
            _wsConnected = false;
            return;
        }
        
        // Drain incoming frames (we don't expect commands via WS, but must read to keep connection alive)
        String msg = _wsHandler.readFrame(_wsClient);
        // Optional: Handle incoming WS commands here if needed
    }
}

void WebServer::handleApiRequest(WiFiClient& client, String method, String uri, String body) {
    // Parse Query Parameters
    String path = uri;
    String query = "";
    int qIdx = uri.indexOf('?');
    if (qIdx != -1) {
        path = uri.substring(0, qIdx);
        query = uri.substring(qIdx + 1);
    }

    // 1. Serve Static UI
    if (path == "/" || path == "/index.html") {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        if (WEB_ASSETS_GZIPPED) {
            client.println("Content-Encoding: gzip");
        }
        client.print("Content-Length: ");
        client.println(INDEX_HTML_SIZE);
        client.println("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
        client.println("Pragma: no-cache");
        client.println("Connection: close");
        client.println();
        
        // Write PROGMEM buffer
        const uint8_t* ptr = INDEX_HTML;
        size_t remaining = INDEX_HTML_SIZE;
        uint8_t buf[64];
        while (remaining > 0) {
            size_t chunk = (remaining > 64) ? 64 : remaining;
            memcpy_P(buf, ptr, chunk);
            client.write(buf, chunk);
            ptr += chunk;
            remaining -= chunk;
        }
        return;
    }

    // 2. API: Profiles
    if (path == "/profiles") {
        if (method == "GET") {
            JsonDocument doc;
            if (query.startsWith("id=")) {
                int id = query.substring(3).toInt();
                SpinProfile p;
                if (_pm.getProfile(id, p)) {
                    doc["id"] = p.id;
                    doc["name"] = p.name;
                    JsonArray steps = doc["steps"].to<JsonArray>();
                    for(int i=0; i<p.stepCount; i++) {
                        JsonObject s = steps.add<JsonObject>();
                        s["startRPM"] = p.steps[i].startRPM;
                        s["targetRPM"] = p.steps[i].targetRPM;
                        s["rampDurationMs"] = p.steps[i].rampDurationMs;
                        s["holdDurationMs"] = p.steps[i].holdDurationMs;
                        s["rampType"] = (int)p.steps[i].rampType;
                    }
                    sendJsonResponse(client, 200, doc);
                } else {
                    client.println("HTTP/1.1 404 Not Found\r\n\r\n");
                }
            } else {
                _pm.getProfileList(doc);
                sendJsonResponse(client, 200, doc);
            }
        } 
        else if (method == "POST") {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (!error) {
                SpinProfile p;
                memset(&p, 0, sizeof(SpinProfile)); // Zero-init to prevent garbage in padding/unused steps
                
                p.id = doc["id"] | 255;
                
                const char* nameStr = doc["name"];
                if (nameStr && strlen(nameStr) > 0) {
                    strncpy(p.name, nameStr, sizeof(p.name) - 1);
                } else {
                    strncpy(p.name, "Unnamed Profile", sizeof(p.name) - 1);
                }
                p.name[sizeof(p.name) - 1] = '\0';
                
                JsonArray steps = doc["steps"];
                p.stepCount = 0;
                for(JsonObject s : steps) {
                    if(p.stepCount >= 20) break;
                    p.steps[p.stepCount].startRPM = s["startRPM"];
                    p.steps[p.stepCount].targetRPM = s["targetRPM"];
                    p.steps[p.stepCount].rampDurationMs = s["rampDurationMs"];
                    p.steps[p.stepCount].holdDurationMs = s["holdDurationMs"];
                    p.steps[p.stepCount].rampType = (RampType)s["rampType"].as<int>();
                    p.stepCount++;
                }
                
                if (_pm.createProfile(p)) {
                    client.println("HTTP/1.1 200 OK\r\n\r\n");
                } else {
                    client.println("HTTP/1.1 500 Error Saving\r\n\r\n");
                }
            } else {
                client.println("HTTP/1.1 400 Bad JSON\r\n\r\n");
            }
        }
        else if (method == "DELETE") {
            if (query.startsWith("id=")) {
                int id = query.substring(3).toInt();
                _pm.deleteProfile(id);
                client.println("HTTP/1.1 200 OK\r\n\r\n");
            }
        }
        return;
    }

    // 3. API: Commands
    if (path.startsWith("/run/")) {
        int id = path.substring(5).toInt();
        Serial.print("CMD: Run Profile ID "); Serial.println(id);
        _engine.runProfile(id);
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }
    
    if (path == "/stop") {
        Serial.println("CMD: Stop");
        _engine.stop();
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }
    
    if (path == "/pause") {
        Serial.println("CMD: Pause");
        _engine.pause();
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }

    // 4. API: Settings
    if (path == "/settings") {
        if (method == "GET") {
            SystemSettings s;
            _storage.loadSettings(s);
            
            JsonDocument doc;
            doc["pid"]["kp"] = s.pid.kp;
            doc["pid"]["ki"] = s.pid.ki;
            doc["pid"]["kd"] = s.pid.kd;
            doc["pid"]["alpha"] = s.filterAlpha;
            doc["pid"]["windup"] = s.windupRange;
            doc["sys"]["maxRPM"] = s.maxRPM;
            doc["sys"]["calibrated"] = s.escCalibrated;
            doc["sys"]["controlMode"] = (int)s.controlMode;
            doc["sys"]["rpmCheck"] = g_rpmCheckEnabled;
            doc["sys"]["motorKV"] = s.motorKV;
            doc["sys"]["batteryVoltage"] = s.batteryVoltage;
            doc["wifi"]["ssid"] = s.wifi.ssid;
            // Don't send password back
            
            sendJsonResponse(client, 200, doc);
        } 
        else if (method == "POST") {
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                SystemSettings s;
                _storage.loadSettings(s); // Load existing to preserve what isn't sent
                
                if (doc.containsKey("pid")) {
                    s.pid.kp = doc["pid"]["kp"];
                    s.pid.ki = doc["pid"]["ki"];
                    s.pid.kd = doc["pid"]["kd"];
                    s.filterAlpha = doc["pid"]["alpha"] | 1.0f;
                    s.windupRange = doc["pid"]["windup"] | 500.0f;
                    
                    // Apply Runtime PID Update
                    escController.setPID(s.pid.kp, s.pid.ki, s.pid.kd);
                    escController.setFilterAlpha(s.filterAlpha);
                    escController.setWindupRange(s.windupRange);
                }
                
                if (doc.containsKey("sys")) {
                    JsonObject sys = doc["sys"];
                    if (sys.containsKey("maxRPM")) s.maxRPM = sys["maxRPM"];
                    if (sys.containsKey("calibrated")) s.escCalibrated = sys["calibrated"];
                    if (sys.containsKey("controlMode")) s.controlMode = (ControlMode)sys["controlMode"].as<int>();
                    if (sys.containsKey("motorKV")) s.motorKV = sys["motorKV"].as<float>();
                    if (sys.containsKey("batteryVoltage")) s.batteryVoltage = sys["batteryVoltage"].as<float>();
                    
                    if (sys.containsKey("rpmCheck")) {
                        g_rpmCheckEnabled = sys["rpmCheck"];
                        EEPROM.put(510, g_rpmCheckEnabled);
                    }
                }

                if (doc.containsKey("wifi")) {
                    const char* ssid = doc["wifi"]["ssid"];
                    const char* pass = doc["wifi"]["pass"];
                    if (ssid && strlen(ssid) > 0) {
                        strncpy(s.wifi.ssid, ssid, 32);
                        if (pass && strlen(pass) > 0) {
                            strncpy(s.wifi.password, pass, 64);
                        }
                        s.wifi.valid = true;
                    }
                }
                
                _storage.saveSettings(s);
                // Apply runtime changes to ESC controller
                
                if (!g_rpmCheckEnabled) {
                    escController.setControlMode(CONTROL_KV);
                } else {
                    escController.setControlMode((int)s.controlMode);
                }
                
                escController.setMotorKV(s.motorKV);
                escController.setBatteryVoltage(s.batteryVoltage);
                client.println("HTTP/1.1 200 OK\r\n\r\n");
            }
        }
        return;
    }

    // 5. API: Factory Reset
    if (path == "/reset" && method == "POST") {
        _storage.wipe();
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }

    // 6. API: Reboot
    if (path == "/reboot" && method == "POST") {
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        client.stop();
        delay(100);
        NVIC_SystemReset();
        return;
    }

    // 7. API: Tune
    if (path == "/tune" && method == "POST") {
        Serial.println("CMD: Start Tuning");
        _engine.startTuning();
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }

    // 8. API: Manual Control
    if (path == "/manual" && method == "POST") {
        JsonDocument doc;
        deserializeJson(doc, body);
        if (doc.containsKey("enable") && doc["enable"].as<bool>()) {
            _engine.startManual();
        }
        if (doc.containsKey("rpm")) {
            _engine.setManualRPM(doc["rpm"]);
        }
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }

    // 9. API: Calibration
    if (path == "/calibrate" && method == "POST") {
        JsonDocument doc;
        deserializeJson(doc, body);
        if (doc.containsKey("throttle")) {
            _engine.setCalibrationThrottle(doc["throttle"]);
        }
        client.println("HTTP/1.1 200 OK\r\n\r\n");
        return;
    }

    // 404
    client.println("HTTP/1.1 404 Not Found\r\n\r\n");
}

void WebServer::sendJsonResponse(WiFiClient& client, int statusCode, const JsonDocument& doc) {
    client.print("HTTP/1.1 ");
    client.print(statusCode);
    client.println(" OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    serializeJson(doc, client);
}

void WebServer::broadcastTelemetry(const TelemetryData& data) {
    if (!_wsConnected || !_wsClient.connected()) return;
    
    JsonDocument doc;
    
    // Map Enum to String for UI
    switch(data.state) {
        case STATE_IDLE: doc["state"] = "IDLE"; break;
        case STATE_RUNNING: doc["state"] = "RUNNING"; break;
        case STATE_PAUSED: doc["state"] = "PAUSED"; break;
        case STATE_STOPPING: doc["state"] = "STOPPING"; break;
        case STATE_ERROR: doc["state"] = "ERROR"; break;
        case STATE_EMERGENCY_STOP: doc["state"] = "EMERGENCY"; break;
        case STATE_TUNING: doc["state"] = "TUNING"; break;
        case STATE_MANUAL: doc["state"] = "MANUAL"; break;
        case STATE_CALIBRATING: doc["state"] = "CALIBRATING"; break;
    }
    
    doc["rpm"] = data.currentRPM;
    doc["target"] = data.targetRPM;
    doc["step"] = data.currentStepIndex + 1; // 1-based for UI
    doc["timeRem"] = data.stepTimeRemaining;
    doc["pId"] = data.profileId;
    
    if (data.errorMessage) {
        doc["error"] = data.errorMessage;
    }
    
    String output;
    serializeJson(doc, output);
    _wsHandler.sendFrame(_wsClient, output);
}