#include "SimpleWebSocket.h"

// Minimal SHA1 implementation for WebSocket handshake
#define SHA1_ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define SHA1_BLK(i) (block[i&15] = SHA1_ROL(block[(i+13)&15] ^ block[(i+8)&15] ^ block[(i+2)&15] ^ block[i&15], 1))

void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e, block[16];
    memcpy(block, buffer, 64);
    
    // Fix endianness (Arduino is Little Endian, SHA1 expects Big Endian words)
    for (int i = 0; i < 16; i++) {
        block[i] = (block[i] << 24) | ((block[i] << 8) & 0x00FF0000) | 
                   ((block[i] >> 8) & 0x0000FF00) | (block[i] >> 24);
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        
        uint32_t temp = SHA1_ROL(a, 5) + f + e + k + (i < 16 ? block[i] : SHA1_BLK(i));
        e = d; d = c; c = SHA1_ROL(b, 30); b = a; a = temp;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void SimpleWebSocket::sha1(const uint8_t* data, size_t length, uint8_t* hash) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t bitlen = (uint64_t)length * 8;
    uint8_t buffer[64];
    size_t idx = 0;

    for (size_t i = 0; i < length; i++) {
        buffer[idx++] = data[i];
        if (idx == 64) {
            sha1_transform(state, buffer);
            idx = 0;
        }
    }

    buffer[idx++] = 0x80;
    if (idx > 56) {
        while (idx < 64) buffer[idx++] = 0;
        sha1_transform(state, buffer);
        idx = 0;
    }
    while (idx < 56) buffer[idx++] = 0;
    
    // Append length in bits (Big Endian)
    for (int i = 7; i >= 0; i--) {
        buffer[56 + i] = bitlen & 0xFF;
        bitlen >>= 8;
    }
    sha1_transform(state, buffer);

    for (int i = 0; i < 5; i++) {
        hash[i*4] = (state[i] >> 24) & 0xFF;
        hash[i*4+1] = (state[i] >> 16) & 0xFF;
        hash[i*4+2] = (state[i] >> 8) & 0xFF;
        hash[i*4+3] = state[i] & 0xFF;
    }
}

// Base64 Table
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

SimpleWebSocket::SimpleWebSocket() {}

bool SimpleWebSocket::isUpgradeRequest(const String& header) {
    String h = header;
    h.toLowerCase();
    return (h.indexOf("upgrade: websocket") != -1);
}

bool SimpleWebSocket::handleHandshake(WiFiClient& client, const String& header) {
    // Extract Key
    int keyStart = header.indexOf("Sec-WebSocket-Key: ");
    if (keyStart == -1) return false;
    keyStart += 19;
    int keyEnd = header.indexOf("\n", keyStart);
    if (keyEnd == -1) keyEnd = header.length();
    String clientKey = header.substring(keyStart, keyEnd);
    clientKey.trim();
    
    String acceptKey = generateAcceptKey(clientKey);
    
    client.print("HTTP/1.1 101 Switching Protocols\r\n");
    client.print("Upgrade: websocket\r\n");
    client.print("Connection: Upgrade\r\n");
    client.print("Sec-WebSocket-Accept: ");
    client.print(acceptKey);
    client.print("\r\n\r\n");
    
    return true;
}

String SimpleWebSocket::generateAcceptKey(String clientKey) {
    String input = clientKey + _guid;
    uint8_t hash[20];
    sha1((const uint8_t*)input.c_str(), input.length(), hash);
    return encodeBase64(hash, 20);
}

String SimpleWebSocket::encodeBase64(const uint8_t* data, size_t length) {
    String ret = "";
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i <4) ; i++) ret += b64_table[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++) ret += b64_table[char_array_4[j]];
        while((i++ < 3)) ret += '=';
    }
    return ret;
}

String SimpleWebSocket::readFrame(WiFiClient& client) {
    if (client.available() < 2) return "";
    
    uint8_t b1 = client.read();
    uint8_t b2 = client.read();
    
    // Opcode: b1 & 0x0F (1=text, 8=close, 9=ping, 10=pong)
    uint8_t opcode = b1 & 0x0F;
    bool masked = b2 & 0x80;
    uint64_t len = b2 & 0x7F;
    
    if (opcode == 0x8) { // Close
        client.stop();
        return "";
    }

    if (opcode == 0x9) { // Ping
        // Respond with Pong (opcode 0xA)
        client.write(0x8A);
        client.write((uint8_t)0); 
        return "";
    }
    
    if (len == 126) {
        while(client.available() < 2) { yield(); }
        len = (client.read() << 8) | client.read();
    } else if (len == 127) {
        return ""; // Not supporting huge frames
    }
    
    uint8_t mask[4];
    if (masked) {
        while(client.available() < 4) { yield(); }
        for (int i=0; i<4; i++) mask[i] = client.read();
    }

    if (opcode != 0x01) return ""; // Only return text payloads
    
    String payload = "";
    payload.reserve(len);
    
    for (uint64_t i=0; i<len; i++) {
        while(!client.available()) yield();
        char c = client.read();
        if (masked) c ^= mask[i % 4];
        payload += c;
    }
    
    return payload;
}

void SimpleWebSocket::sendFrame(WiFiClient& client, const String& payload) {
    sendFrame(client, payload.c_str());
}

void SimpleWebSocket::sendFrame(WiFiClient& client, const char* payload) {
    if (!client.connected()) return;
    
    size_t len = strlen(payload);
    
    client.write(0x81); // FIN + Text
    
    if (len <= 125) {
        client.write((uint8_t)len);
    } else if (len <= 65535) {
        client.write(126);
        client.write((len >> 8) & 0xFF);
        client.write(len & 0xFF);
    }
    
    client.write(payload, len);
}