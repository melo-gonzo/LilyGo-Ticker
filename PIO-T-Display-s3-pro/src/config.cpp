#include "config.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <lvgl.h>

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
int BARS_TO_SHOW = 50;

// Network configuration - Use your specified defaults
bool USE_STATIC_IP = true;
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

Preferences preferences;

void loadConfig() {
    preferences.begin("stocktracker", true); // read-only mode
    
    USE_TEST_DATA = preferences.getBool("useTestData", false);
    USE_INTRADAY_DATA = true; // Always enable for real-time updates
    INTRADAY_UPDATE_INTERVAL = preferences.getInt("updateInterval", 1);
    CANDLE_COLLECTION_DURATION = preferences.getInt("candleDuration", 180);
    STOCK_SYMBOL = preferences.getString("symbol", "SPY");
    ENFORCE_MARKET_HOURS = preferences.getBool("enforceHours", true);
    YAHOO_INTERVAL = preferences.getString("yahooInterval", "1m");
    YAHOO_RANGE = preferences.getString("yahooRange", "1d");
    BARS_TO_SHOW = preferences.getInt("barsToShow", 50);
    
    // Network configuration with your defaults
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
    
    // Auto-sync candle duration with interval on startup
    syncCandleDurationWithInterval();
    
    // Validate bars to show using actual screen dimensions
    int actualScreenWidth = getScreenWidth();
    int maxBars = calculateMaxBars(actualScreenWidth, INFO_PANEL_WIDTH, 1);
    
    if (BARS_TO_SHOW < 1 || BARS_TO_SHOW > maxBars) {
        BARS_TO_SHOW = min(50, maxBars);
        Serial.println("BARS_TO_SHOW was out of range, adjusted to: " + String(BARS_TO_SHOW));
    }
    
    Serial.println("Configuration loaded:");
    Serial.println("Symbol: " + STOCK_SYMBOL);
    Serial.println("Interval: " + YAHOO_INTERVAL);
    Serial.println("Range: " + YAHOO_RANGE);
    Serial.println("Auto-synced candle duration: " + String(CANDLE_COLLECTION_DURATION) + " seconds");
    Serial.println("Bars to show: " + String(BARS_TO_SHOW) + " (max: " + String(maxBars) + ")");
    Serial.println("Screen width: " + String(actualScreenWidth));
    Serial.println("Use Test Data: " + String(USE_TEST_DATA));
    Serial.println("Use Intraday: " + String(USE_INTRADAY_DATA) + " (always enabled)");
    Serial.println("Network - Use Static IP: " + String(USE_STATIC_IP));
    if (USE_STATIC_IP) {
        Serial.println("Static IP: " + STATIC_IP);
        Serial.println("Gateway: " + GATEWAY_IP);
        Serial.println("Subnet: " + SUBNET_MASK);
    }
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

bool validateSymbol(const String& symbol) {
    // Basic validation for stock symbols
    if (symbol.length() == 0 || symbol.length() > 8) {
        return false;
    }
    
    // Must contain at least one letter
    bool hasLetter = false;
    for (int i = 0; i < symbol.length(); i++) {
        char c = symbol.charAt(i);
        if (isalpha(c)) {
            hasLetter = true;
        } else if (!isalnum(c) && c != '-') {
            // Only allow alphanumeric characters and hyphens (for crypto pairs)
            return false;
        }
    }
    
    return hasLetter;
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

bool validateIP(const String& ip) {
    // Basic IP validation - check for 4 parts separated by dots
    int dotCount = 0;
    int partStart = 0;
    
    for (int i = 0; i <= ip.length(); i++) {
        if (i == ip.length() || ip.charAt(i) == '.') {
            if (i == partStart) return false; // Empty part
            
            String part = ip.substring(partStart, i);
            int value = part.toInt();
            
            // Check if conversion was successful and value is in valid range
            if (part != String(value) || value < 0 || value > 255) {
                return false;
            }
            
            dotCount++;
            partStart = i + 1;
        }
    }
    
    return dotCount == 4; // Should have exactly 4 parts
}

int calculateMaxBars(int screenWidth, int panelWidth, int candleMinWidth) {
    int chartWidth = screenWidth - panelWidth;
    
    // FIXED: Proper calculation that allows 1-pixel candles
    int maxBars;
    int padding = CANDLE_PADDING;
    
    if (padding == 0) {
        // Simple case: no padding, just divide available width by minimum candle width
        maxBars = chartWidth / candleMinWidth;
    } else {
        // With padding: solve the equation
        // chartWidth = (candleWidth * numBars) + (padding * (numBars - 1))
        // chartWidth = numBars * (candleWidth + padding) - padding
        // numBars = (chartWidth + padding) / (candleWidth + padding)
        maxBars = (chartWidth + padding) / (candleMinWidth + padding);
    }
    
    // FIXED: More reasonable bounds - allow more bars for higher resolution displays
    maxBars = std::max(5, maxBars);   // Minimum of 5 bars (was 10)
    maxBars = std::min(1000, maxBars); // Maximum of 1000 bars (was 500)
    
    Serial.println("calculateMaxBars:");
    Serial.println("  Screen width: " + String(screenWidth));
    Serial.println("  Panel width: " + String(panelWidth)); 
    Serial.println("  Chart width: " + String(chartWidth));
    Serial.println("  Min candle width: " + String(candleMinWidth));
    Serial.println("  Padding: " + String(padding));
    Serial.println("  Calculated max bars: " + String(maxBars));
    
    // Additional debug: show what the actual candle width would be
    int actual_candle_width;
    if (padding == 0) {
        actual_candle_width = chartWidth / maxBars;
    } else {
        int total_padding_space = (maxBars - 1) * padding;
        actual_candle_width = (chartWidth - total_padding_space) / maxBars;
    }
    Serial.println("  Actual candle width at max bars: " + String(actual_candle_width));
    
    return maxBars;
}

int getScreenWidth() {
    lv_disp_t* disp = lv_disp_get_default();
    if (disp != NULL) {
        return lv_disp_get_hor_res(disp);
    }
    return 320; // Fallback for T-Display-AMOLED
}

void syncCandleDurationWithInterval() {
    int intervalSeconds = 0;
    
    if (YAHOO_INTERVAL == "1m") intervalSeconds = 60;
    else if (YAHOO_INTERVAL == "2m") intervalSeconds = 120;
    else if (YAHOO_INTERVAL == "5m") intervalSeconds = 300;
    else if (YAHOO_INTERVAL == "15m") intervalSeconds = 900;
    else if (YAHOO_INTERVAL == "30m") intervalSeconds = 1800;
    else if (YAHOO_INTERVAL == "60m" || YAHOO_INTERVAL == "1h") intervalSeconds = 3600;
    else if (YAHOO_INTERVAL == "90m") intervalSeconds = 5400;
    else if (YAHOO_INTERVAL == "1d") intervalSeconds = 86400;
    else if (YAHOO_INTERVAL == "5d") intervalSeconds = 432000; // 5 days
    else if (YAHOO_INTERVAL == "1wk") intervalSeconds = 604800; // 1 week
    else if (YAHOO_INTERVAL == "1mo") intervalSeconds = 2592000; // 30 days (approx)
    else if (YAHOO_INTERVAL == "3mo") intervalSeconds = 7776000; // 90 days (approx)
    else {
        // For unsupported intervals, default to 5 minutes
        Serial.println("Warning: Unsupported interval, defaulting to 5 minutes");
        intervalSeconds = 300;
    }
    
    if (intervalSeconds != CANDLE_COLLECTION_DURATION) {
        Serial.printf("Auto-syncing candle duration: %d -> %d seconds (%s)\n", 
                     CANDLE_COLLECTION_DURATION, intervalSeconds, YAHOO_INTERVAL.c_str());
        CANDLE_COLLECTION_DURATION = intervalSeconds;
    }
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
    
    // Always enable intraday data for real-time updates
    USE_INTRADAY_DATA = true;
    Serial.println("Intraday data always enabled for real-time updates");
    
    if (doc["updateInterval"].is<int>()) {
        INTRADAY_UPDATE_INTERVAL = doc["updateInterval"];
    }
    
    // Remove candleDuration handling - it will be auto-synced
    
    if (doc["symbol"].is<String>()) {
        String symbol = doc["symbol"].as<String>();
        symbol.toUpperCase(); // Ensure it's uppercase
        if (validateSymbol(symbol)) {
            STOCK_SYMBOL = symbol;
            Serial.println("Symbol updated to: " + STOCK_SYMBOL);
        } else {
            Serial.println("Invalid symbol rejected: " + symbol);
        }
    }
    if (doc["enforceHours"].is<bool>()) {
        ENFORCE_MARKET_HOURS = doc["enforceHours"];
    }
    if (doc["yahooInterval"].is<String>()) {
        String interval = doc["yahooInterval"].as<String>();
        if (validateInterval(interval)) {
            YAHOO_INTERVAL = interval;
            syncCandleDurationWithInterval(); // Auto-sync duration with interval
            Serial.printf("Interval updated to: %s (duration: %d seconds)\n", 
                         YAHOO_INTERVAL.c_str(), CANDLE_COLLECTION_DURATION);
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
        int maxBarsScreen = calculateMaxBars(getScreenWidth(), INFO_PANEL_WIDTH, 1);
        int maxBarsData = MAX_CANDLES; // Data buffer limitation
        int actualMaxBars = std::min(maxBarsScreen, maxBarsData);
        
        if (bars >= 1 && bars <= actualMaxBars) {
            BARS_TO_SHOW = bars;
            Serial.printf("Bars to show set to: %d (max screen: %d, max data: %d)\n", 
                         BARS_TO_SHOW, maxBarsScreen, maxBarsData);
        } else {
            // Clamp to valid range
            BARS_TO_SHOW = std::min(bars, actualMaxBars);
            BARS_TO_SHOW = std::max(BARS_TO_SHOW, 1);
            Serial.printf("Bars clamped from %d to %d (max allowed: %d)\n", 
                         bars, BARS_TO_SHOW, actualMaxBars);
        }
    }
    if (doc["useStaticIP"].is<bool>()) {
        USE_STATIC_IP = doc["useStaticIP"];
    }
    if (doc["staticIP"].is<String>()) {
        String ip = doc["staticIP"].as<String>();
        if (validateIP(ip)) {
            STATIC_IP = ip;
        }
    }
    if (doc["gatewayIP"].is<String>()) {
        String ip = doc["gatewayIP"].as<String>();
        if (validateIP(ip)) {
            GATEWAY_IP = ip;
        }
    }
    if (doc["subnetMask"].is<String>()) {
        String mask = doc["subnetMask"].as<String>();
        if (validateIP(mask)) {  // subnet mask follows same format as IP
            SUBNET_MASK = mask;
        }
    }
    
    // Save the updated configuration
    saveConfig();
    return true;
}

// Updated getConfigJSON function - remove candleDuration
String getConfigJSON() {
    JsonDocument doc;
    
    doc["useTestData"] = USE_TEST_DATA;
    doc["useIntraday"] = USE_INTRADAY_DATA;
    doc["updateInterval"] = INTRADAY_UPDATE_INTERVAL;
    // Remove candleDuration from JSON response
    doc["symbol"] = STOCK_SYMBOL;
    doc["enforceHours"] = ENFORCE_MARKET_HOURS;
    doc["yahooInterval"] = YAHOO_INTERVAL;
    doc["yahooRange"] = YAHOO_RANGE;
    doc["barsToShow"] = BARS_TO_SHOW;
    
    // Add computed candle duration for display purposes (read-only)
    doc["computedCandleDuration"] = CANDLE_COLLECTION_DURATION;
    
    // Calculate actual maximum bars considering both screen and data limitations
    int actualScreenWidth = getScreenWidth();
    int maxBarsScreen = calculateMaxBars(actualScreenWidth, INFO_PANEL_WIDTH, 1);
    int maxBarsData = MAX_CANDLES; // Data buffer limitation
    int actualMaxBars = std::min(maxBarsScreen, maxBarsData);
    
    doc["maxBars"] = actualMaxBars;
    doc["maxBarsScreen"] = maxBarsScreen; // For debugging
    doc["maxBarsData"] = maxBarsData;     // For debugging
    doc["screenWidth"] = actualScreenWidth;
    
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
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}