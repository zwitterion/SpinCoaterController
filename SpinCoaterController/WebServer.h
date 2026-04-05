#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFiS3.h>
#include <ArduinoJson.h>
#include "SimpleWebSocket.h"
#include "ProfileManager.h"
#include "ExecutionEngine.h"
#include "EEPROMStorage.h"
#include "WiFiManager.h"
#include "web_assets.h"

class WebServer {
public:
    WebServer(int port, ProfileManager& pm, ExecutionEngine& engine, EEPROMStorage& storage, WiFiManager& wifi);
    
    void begin();
    void update();
    
    // Broadcast telemetry to connected websocket clients
    void broadcastTelemetry(const TelemetryData& data);

private:
    WiFiServer _server;
    WiFiClient _wsClient; // Single client support for simplicity, or array for multiple
    bool _wsConnected;

    // HTTP State Machine Members
    WiFiClient _httpClient;
    int _httpState;
    String _headerBuf;
    String _bodyBuf;
    int _contentLength;
    String _method;
    String _uri;
    
    SimpleWebSocket _wsHandler;
    
    ProfileManager& _pm;
    ExecutionEngine& _engine;
    EEPROMStorage& _storage;
    WiFiManager& _wifi;

    void handleHttpClient(WiFiClient& client);
    void handleWebSocket();
    
    // API Handlers
    void handleApiRequest(WiFiClient& client, String method, String endpoint, String body);
    void sendJsonResponse(WiFiClient& client, int statusCode, const JsonDocument& doc);
};

#endif