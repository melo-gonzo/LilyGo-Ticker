// WebSocket.h

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

// Externally defined function to update the UI
void updateStockUI(float btcRate, float highRate, float lowRate);

// Function to initialize and connect the WebSocket client
void initWebSocket();

// Function to handle the WebSocket loop
void handleWebSocket();

#endif // WEBSOCKET_H
