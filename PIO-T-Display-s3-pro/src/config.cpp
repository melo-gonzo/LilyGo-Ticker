#include "config.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// Default configuration values
bool USE_TEST_DATA = false;
bool USE_INTRADAY_DATA = true;
int INTRADAY_UPDATE_INTERVAL = 1;
int CANDLE_COLLECTION_DURATION = 180;
String STOCK_SYMBOL = "SPY";
bool ENFORCE_MARKET_HOURS = true;

// Yahoo Finance API parameters
String YAHOO_INTERVAL = "1m";  // Default to 1 minute intervals
String YAHOO_RANGE = "1d";     // Default to 1 day range

// Chart display configuration
int BARS_TO_SHOW = 50;  // Added missing definition

// Network configuration
bool USE_STATIC_IP = false;
String STATIC_IP = "192.168.4.184";
String GATEWAY_IP = "192.168.4.1";
String SUBNET_MASK = "255.255.255.0";

// Valid options for validation and web interface
const char* VALID_INTERVALS[] = {
    "1m", "2m", "5m", "15m", "30m", "60m", "90m", "1h", "1d", "5d", "1wk", "1mo", "3mo"
};
const int VALID_INTERVALS_COUNT = sizeof(VALID_INTERVALS) / sizeof(VALID_INTERVALS[0]);

const char* VALID_RANGES[] = {
    "1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "ytd", "max"
};
const int VALID_RANGES_COUNT = sizeof(VALID_RANGES) / sizeof(VALID_RANGES[0]);

const char* VALID_SYMBOLS[] = {
    "SPY", "QQQ", "NVDA", "AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "META", "BTC-USD", "ETH-USD"
};
const int VALID_SYMBOLS_COUNT = sizeof(VALID_SYMBOLS) / sizeof(VALID_SYMBOLS[0]);

Preferences preferences;

void loadConfig() {
    preferences.begin("stocktracker", true); // read-only mode
    
    USE_TEST_DATA = preferences.getBool("useTestData", false);
    USE_INTRADAY_DATA = preferences.getBool("useIntraday", true);
    INTRADAY_UPDATE_INTERVAL = preferences.getInt("updateInterval", 1);
    CANDLE_COLLECTION_DURATION = preferences.getInt("candleDuration", 180);
    STOCK_SYMBOL = preferences.getString("symbol", "SPY");
    ENFORCE_MARKET_HOURS = preferences.getBool("enforceHours", true);
    YAHOO_INTERVAL = preferences.getString("yahooInterval", "1m");
    YAHOO_RANGE = preferences.getString("yahooRange", "1d");
    BARS_TO_SHOW = preferences.getInt("barsToShow", 50);
    USE_STATIC_IP = preferences.getBool("useStaticIP", false);
    STATIC_IP = preferences.getString("staticIP", "192.168.4.184");
    GATEWAY_IP = preferences.getString("gatewayIP", "192.168.4.1");
    SUBNET_MASK = preferences.getString("subnetMask", "255.255.255.0");
    
    preferences.end();
    
    // Validate loaded values
    if (!validateInterval(YAHOO_INTERVAL)) {
        YAHOO_INTERVAL = "1m";
    }
    if (!validateRange(YAHOO_RANGE)) {
        YAHOO_RANGE = "1d";
    }
    
    // Validate bars to show (assume 320px width, 80px panel = 240px chart area)
    int maxBars = calculateMaxBars(320, 80, 2);
    if (BARS_TO_SHOW < 1 || BARS_TO_SHOW > maxBars) {
        BARS_TO_SHOW = min(50, maxBars);
    }
    
    Serial.println("Configuration loaded:");
    Serial.println("Symbol: " + STOCK_SYMBOL);
    Serial.println("Interval: " + YAHOO_INTERVAL);
    Serial.println("Range: " + YAHOO_RANGE);
    Serial.println("Bars to show: " + String(BARS_TO_SHOW));
    Serial.println("Use Test Data: " + String(USE_TEST_DATA));
    Serial.println("Use Intraday: " + String(USE_INTRADAY_DATA));
}

void saveConfig() {
    preferences.begin("stocktracker", false); // read-write mode
    
    preferences.putBool("useTestData", USE_TEST_DATA);
    preferences.putBool("useIntraday", USE_INTRADAY_DATA);
    preferences.putInt("updateInterval", INTRADAY_UPDATE_INTERVAL);
    preferences.putInt("candleDuration", CANDLE_COLLECTION_DURATION);
    preferences.putString("symbol", STOCK_SYMBOL);
    preferences.putBool("enforceHours", ENFORCE_MARKET_HOURS);
    preferences.putString("yahooInterval", YAHOO_INTERVAL);
    preferences.putString("yahooRange", YAHOO_RANGE);
    preferences.putInt("barsToShow", BARS_TO_SHOW);
    preferences.putBool("useStaticIP", USE_STATIC_IP);
    preferences.putString("staticIP", STATIC_IP);
    preferences.putString("gatewayIP", GATEWAY_IP);
    preferences.putString("subnetMask", SUBNET_MASK);
    
    preferences.end();
    
    Serial.println("Configuration saved");
}

bool validateInterval(const String& interval) {
    for (int i = 0; i < VALID_INTERVALS_COUNT; i++) {
        if (interval == VALID_INTERVALS[i]) {
            return true;
        }
    }
    return false;
}

bool validateRange(const String& range) {
    for (int i = 0; i < VALID_RANGES_COUNT; i++) {
        if (range == VALID_RANGES[i]) {
            return true;
        }
    }
    return false;
}

int calculateMaxBars(int screenWidth, int panelWidth, int candleMinWidth) {
    int chartWidth = screenWidth - panelWidth;
    return chartWidth / candleMinWidth;
}

String getConfigJSON() {
    JsonDocument doc;
    
    doc["useTestData"] = USE_TEST_DATA;
    doc["useIntraday"] = USE_INTRADAY_DATA;
    doc["updateInterval"] = INTRADAY_UPDATE_INTERVAL;
    doc["candleDuration"] = CANDLE_COLLECTION_DURATION;
    doc["symbol"] = STOCK_SYMBOL;
    doc["enforceHours"] = ENFORCE_MARKET_HOURS;
    doc["yahooInterval"] = YAHOO_INTERVAL;
    doc["yahooRange"] = YAHOO_RANGE;
    doc["barsToShow"] = BARS_TO_SHOW;
    doc["maxBars"] = calculateMaxBars(320, 80, 2); // Provide max for validation
    doc["useStaticIP"] = USE_STATIC_IP;
    doc["staticIP"] = STATIC_IP;
    doc["gatewayIP"] = GATEWAY_IP;
    doc["subnetMask"] = SUBNET_MASK;
    
    // Add valid options for dropdowns
    JsonArray intervals = doc["validIntervals"].to<JsonArray>();
    for (int i = 0; i < VALID_INTERVALS_COUNT; i++) {
        intervals.add(VALID_INTERVALS[i]);
    }
    
    JsonArray ranges = doc["validRanges"].to<JsonArray>();
    for (int i = 0; i < VALID_RANGES_COUNT; i++) {
        ranges.add(VALID_RANGES[i]);
    }
    
    JsonArray symbols = doc["validSymbols"].to<JsonArray>();
    for (int i = 0; i < VALID_SYMBOLS_COUNT; i++) {
        symbols.add(VALID_SYMBOLS[i]);
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

bool setConfigFromJSON(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.println("Failed to parse JSON config");
        return false;
    }
    
    // Update configuration values
    if (doc["useTestData"].is<bool>()) {
        USE_TEST_DATA = doc["useTestData"];
    }
    if (doc["useIntraday"].is<bool>()) {
        USE_INTRADAY_DATA = doc["useIntraday"];
    }
    if (doc["updateInterval"].is<int>()) {
        INTRADAY_UPDATE_INTERVAL = doc["updateInterval"];
    }
    if (doc["candleDuration"].is<int>()) {
        CANDLE_COLLECTION_DURATION = doc["candleDuration"];
    }
    if (doc["symbol"].is<String>()) {
        STOCK_SYMBOL = doc["symbol"].as<String>();
    }
    if (doc["enforceHours"].is<bool>()) {
        ENFORCE_MARKET_HOURS = doc["enforceHours"];
    }
    if (doc["yahooInterval"].is<String>()) {
        String interval = doc["yahooInterval"].as<String>();
        if (validateInterval(interval)) {
            YAHOO_INTERVAL = interval;
        }
    }
    if (doc["yahooRange"].is<String>()) {
        String range = doc["yahooRange"].as<String>();
        if (validateRange(range)) {
            YAHOO_RANGE = range;
        }
    }
    if (doc["barsToShow"].is<int>()) {
        int bars = doc["barsToShow"];
        int maxBars = calculateMaxBars(320, 80, 2);
        if (bars >= 1 && bars <= maxBars) {
            BARS_TO_SHOW = bars;
        }
    }
    if (doc["useStaticIP"].is<bool>()) {
        USE_STATIC_IP = doc["useStaticIP"];
    }
    if (doc["staticIP"].is<String>()) {
        STATIC_IP = doc["staticIP"].as<String>();
    }
    if (doc["gatewayIP"].is<String>()) {
        GATEWAY_IP = doc["gatewayIP"].as<String>();
    }
    if (doc["subnetMask"].is<String>()) {
        SUBNET_MASK = doc["subnetMask"].as<String>();
    }
    
    // Save the updated configuration
    saveConfig();
    return true;
}