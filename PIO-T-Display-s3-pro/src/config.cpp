#include "config.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// Default configuration values
bool USE_TEST_DATA = false;
bool USE_INTRADAY_DATA = true;
int INTRADAY_UPDATE_INTERVAL = 1;
int CANDLE_COLLECTION_DURATION = 180;
String STOCK_SYMBOL = "SPY";
bool ENFORCE_MARKET_HOURS = false;

// Yahoo Finance API parameters
String YAHOO_INTERVAL = "1m";  // Default to 1 minute intervals
String YAHOO_RANGE = "1d";     // Default to 1 day range

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
    
    preferences.end();
    
    // Validate loaded values
    if (!validateInterval(YAHOO_INTERVAL)) {
        YAHOO_INTERVAL = "1m";
    }
    if (!validateRange(YAHOO_RANGE)) {
        YAHOO_RANGE = "1d";
    }
    
    Serial.println("Configuration loaded:");
    Serial.println("Symbol: " + STOCK_SYMBOL);
    Serial.println("Interval: " + YAHOO_INTERVAL);
    Serial.println("Range: " + YAHOO_RANGE);
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
    
    // Add valid options for dropdowns
    JsonArray intervals = doc.createNestedArray("validIntervals");
    for (int i = 0; i < VALID_INTERVALS_COUNT; i++) {
        intervals.add(VALID_INTERVALS[i]);
    }
    
    JsonArray ranges = doc.createNestedArray("validRanges");
    for (int i = 0; i < VALID_RANGES_COUNT; i++) {
        ranges.add(VALID_RANGES[i]);
    }
    
    JsonArray symbols = doc.createNestedArray("validSymbols");
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
    if (doc.containsKey("useTestData")) {
        USE_TEST_DATA = doc["useTestData"];
    }
    if (doc.containsKey("useIntraday")) {
        USE_INTRADAY_DATA = doc["useIntraday"];
    }
    if (doc.containsKey("updateInterval")) {
        INTRADAY_UPDATE_INTERVAL = doc["updateInterval"];
    }
    if (doc.containsKey("candleDuration")) {
        CANDLE_COLLECTION_DURATION = doc["candleDuration"];
    }
    if (doc.containsKey("symbol")) {
        STOCK_SYMBOL = doc["symbol"].as<String>();
    }
    if (doc.containsKey("enforceHours")) {
        ENFORCE_MARKET_HOURS = doc["enforceHours"];
    }
    if (doc.containsKey("yahooInterval")) {
        String interval = doc["yahooInterval"].as<String>();
        if (validateInterval(interval)) {
            YAHOO_INTERVAL = interval;
        }
    }
    if (doc.containsKey("yahooRange")) {
        String range = doc["yahooRange"].as<String>();
        if (validateRange(range)) {
            YAHOO_RANGE = range;
        }
    }
    
    // Save the updated configuration
    saveConfig();
    return true;
}

//// Test intraday with fake data
// bool USE_TEST_DATA = true;
// bool USE_INTRADAY_DATA = true;
// int INTRADAY_UPDATE_INTERVAL = 1; // 5 // 1 
// int CANDLE_COLLECTION_DURATION = 6; // 60 // 6
// const char* STOCK_SYMBOL = "SPY";
// bool ENFORCE_MARKET_HOURS = false;  

//// Daily data
// bool USE_TEST_DATA = false;
// bool USE_INTRADAY_DATA = false;
// int INTRADAY_UPDATE_INTERVAL = 5; // 5
// int CANDLE_COLLECTION_DURATION = 60; // 60
// const char* STOCK_SYMBOL = "SPY";
// bool ENFORCE_MARKET_HOURS = true;  


