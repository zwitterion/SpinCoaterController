#ifndef SIMPLE_WEBSOCKET_H
#define SIMPLE_WEBSOCKET_H

#include <Arduino.h>
#include <WiFiS3.h>

// A lightweight WebSocket implementation compatible with WiFiS3
class SimpleWebSocket {
public:
    SimpleWebSocket();
    
    // Checks if a client is requesting a websocket upgrade
    bool isUpgradeRequest(const String& header);
    
    // Performs the handshake with the client
    bool handleHandshake(WiFiClient& client, const String& header);
    
    // Reads a frame from the client. Returns payload string or empty if no complete frame.
    String readFrame(WiFiClient& client);
    
    // Sends a text frame to the client
    void sendFrame(WiFiClient& client, const String& payload);
    
    // Sends a text frame to the client (char array)
    void sendFrame(WiFiClient& client, const char* payload);

private:
    // Helper to generate the Sec-WebSocket-Accept key
    String generateAcceptKey(String clientKey);
    
    // Base64 encoding helper
    String encodeBase64(const uint8_t* data, size_t length);
    
    // SHA1 helper (using internal ESP32 or software implementation if needed, 
    // but R4 WiFi has ESP32-S3 which supports SHA1, or we use a simple soft-SHA1)
    void sha1(const uint8_t* data, size_t length, uint8_t* hash);
    
    const char* _guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
};

#endif