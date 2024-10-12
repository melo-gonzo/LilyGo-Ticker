#include "WebSocket.h"
#include <WiFi.h>

WebSocketsClient webSocket;

float btcRate = 0.0;
float highRate = 0.0;
float lowRate = 0.0;

void onWebSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
    case WStype_TEXT: {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
        }

        if (doc["e"].is<const char*>() && strcmp(doc["e"].as<const char*>(), "24hrTicker") == 0) {
            btcRate = doc["c"].as<float>();  // Last price
            highRate = doc["h"].as<float>(); // High price
            lowRate = doc["l"].as<float>();  // Low price

            updateStockUI(btcRate, highRate, lowRate);
        }

        break;
    }
    case WStype_DISCONNECTED:
        Serial.println("WebSocket Disconnected");
        break;
    case WStype_CONNECTED:
        Serial.println("WebSocket Connected");
        webSocket.sendTXT("{\"method\": \"SUBSCRIBE\", \"params\": [\"btcusdt@ticker\"], \"id\": 1}");
        break;
    case WStype_ERROR:
        Serial.println("WebSocket Error");
        break;
    default:
        break;
    }
}

void initWebSocket() {
    webSocket.beginSSL("stream.binance.com", 9443, "/ws");
    webSocket.onEvent(onWebSocketEvent);
}

void handleWebSocket() {
    webSocket.loop();
}