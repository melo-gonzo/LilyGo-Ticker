// WebSocket.h

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Externally defined function to update the UI
void updateBitcoinUI(float btcRate, float highRate, float lowRate);

// Function to initialize and connect the WebSocket client
void initWebSocket();

// Function to handle the WebSocket loop
void handleWebSocket();

#endif // WEBSOCKET_H
