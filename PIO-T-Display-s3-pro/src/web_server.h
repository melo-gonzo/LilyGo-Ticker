#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <Arduino.h>

class StockWebServer {
private:
    static WebServer server;
    static bool serverStarted;
    
    // Handler functions
    static void handleRoot();
    static void handleGetConfig();
    static void handleSetConfig();
    static void handleNotFound();
    static String generateHTML();
    
public:
    static bool begin(int port = 80);
    static void handleClient();
    static void stop();
    static bool isRunning() { return serverStarted; }
};

#endif // WEB_SERVER_H