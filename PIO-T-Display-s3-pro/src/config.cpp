#include "config.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <lvgl.h>

// Default configuration values
bool USE_TEST_DATA = false;
bool USE_INTRADAY_DATA = true;
int INTRADAY_UPDATE_INTERVAL = 1000; // Default 1 second in milliseconds
int CANDLE_COLLECTION_DURATION = 180;
String STOCK_SYMBOL = "SPY";
bool ENFORCE_MARKET_HOURS = true;

// Yahoo Finance API parameters
String YAHOO_INTERVAL = "1m"; // Default to 1 minute intervals
String YAHOO_RANGE = "1d";    // Default to 1 day range

// Chart display configuration
int BARS_TO_SHOW = 50;
int TEST_DATA_UPDATES_PER_BAR = 10; // Default: 10 updates per bar

// Network configuration - Use your specified defaults
bool USE_STATIC_IP = true;
String STATIC_IP = "192.168.4.184";
String GATEWAY_IP = "192.168.4.1";
String SUBNET_MASK = "255.255.255.0";

// Valid options for validation and web interface
const char *VALID_INTERVALS[] = {"1m", "2m", "5m", "15m", "30m", "60m", "90m",
                                 "1h", "1d", "5d", "1wk", "1mo", "3mo"};
const int VALID_INTERVALS_COUNT =
    sizeof(VALID_INTERVALS) / sizeof(VALID_INTERVALS[0]);

const char *VALID_RANGES[] = {"1d", "5d", "1mo", "3mo", "6mo", "1y",
                              "2y", "5y", "10y", "ytd", "max"};
const int VALID_RANGES_COUNT = sizeof(VALID_RANGES) / sizeof(VALID_RANGES[0]);

Preferences preferences;

void loadConfig() {
  preferences.begin("stocktracker", true); // read-only mode

  USE_TEST_DATA = preferences.getBool("useTestData", false);
  USE_INTRADAY_DATA = true; // Always enable for real-time updates
  INTRADAY_UPDATE_INTERVAL =
      preferences.getInt("updateInterval", 1000); // Now in milliseconds
  CANDLE_COLLECTION_DURATION = preferences.getInt("candleDuration", 180);
  STOCK_SYMBOL = preferences.getString("symbol", "SPY");
  ENFORCE_MARKET_HOURS = preferences.getBool("enforceHours", true);
  YAHOO_INTERVAL = preferences.getString("yahooInterval", "1m");
  YAHOO_RANGE = preferences.getString("yahooRange", "1d");
  BARS_TO_SHOW =
      preferences.getInt("barsToShow", 50); // Load potentially invalid value
  TEST_DATA_UPDATES_PER_BAR = preferences.getInt("testUpdatesPerBar", 10);

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

  // Validate test data updates per bar
  if (TEST_DATA_UPDATES_PER_BAR < 1) {
    TEST_DATA_UPDATES_PER_BAR = 1;
  } else if (TEST_DATA_UPDATES_PER_BAR > 1000) {
    TEST_DATA_UPDATES_PER_BAR = 1000;
  }

  // Auto-sync candle duration with interval on startup
  syncCandleDurationWithInterval();

  // FIXED: Validate bars to show using calculateMaxBars
  Serial.println("Validating BARS_TO_SHOW...");
  Serial.println("Loaded BARS_TO_SHOW from preferences: " +
                 String(BARS_TO_SHOW));

  int actualScreenWidth = getScreenWidth();
  Serial.println("Screen width: " + String(actualScreenWidth));

  // This should trigger the calculateMaxBars debug output
  int maxBars = calculateMaxBars(actualScreenWidth, INFO_PANEL_WIDTH, 1);
  Serial.println("calculateMaxBars returned: " + String(maxBars));

  if (BARS_TO_SHOW < 1 || BARS_TO_SHOW > maxBars) {
    int oldValue = BARS_TO_SHOW;
    BARS_TO_SHOW = std::min(
        50, maxBars); // Default to 50 or max allowed, whichever is smaller
    Serial.println("BARS_TO_SHOW was out of range (" + String(oldValue) +
                   "), adjusted to: " + String(BARS_TO_SHOW));
    Serial.println("Maximum allowed: " + String(maxBars));

    // Save the corrected value back to preferences
    preferences.begin("stocktracker", false);
    preferences.putInt("barsToShow", BARS_TO_SHOW);
    preferences.end();
    Serial.println("Saved corrected BARS_TO_SHOW to preferences");
  } else {
    Serial.println("BARS_TO_SHOW is valid: " + String(BARS_TO_SHOW) +
                   " (max: " + String(maxBars) + ")");
  }

  // DEBUG: Print the actual stored values
  Serial.println("\n=== CONFIGURATION LOADED ===");
  Serial.println("Symbol: " + STOCK_SYMBOL);
  Serial.println("Interval: " + YAHOO_INTERVAL);
  Serial.println("Range: " + YAHOO_RANGE);
  Serial.println("UPDATE INTERVAL (ms): " + String(INTRADAY_UPDATE_INTERVAL));
  Serial.println("TEST UPDATES PER BAR: " + String(TEST_DATA_UPDATES_PER_BAR));
  Serial.println("Use Test Data: " + String(USE_TEST_DATA));
  Serial.println("Auto-synced candle duration: " +
                 String(CANDLE_COLLECTION_DURATION) + " seconds");
  Serial.println("BARS_TO_SHOW: " + String(BARS_TO_SHOW) + " (validated)");
  Serial.println("Screen width: " + String(actualScreenWidth));
  Serial.println("Use Intraday: " + String(USE_INTRADAY_DATA) +
                 " (always enabled)");
  Serial.println("Network - Use Static IP: " + String(USE_STATIC_IP));
  if (USE_STATIC_IP) {
    Serial.println("Static IP: " + STATIC_IP);
    Serial.println("Gateway: " + GATEWAY_IP);
    Serial.println("Subnet: " + SUBNET_MASK);
  }
  Serial.println("============================\n");

  // Call the bar limitations debug function
  printBarLimitations();
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
  preferences.putInt("testUpdatesPerBar", TEST_DATA_UPDATES_PER_BAR); // NEW
  preferences.putBool("useStaticIP", USE_STATIC_IP);
  preferences.putString("staticIP", STATIC_IP);
  preferences.putString("gatewayIP", GATEWAY_IP);
  preferences.putString("subnetMask", SUBNET_MASK);

  preferences.end();

  Serial.println("Configuration saved");
}

bool validateSymbol(const String &symbol) {
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

bool validateInterval(const String &interval) {
  for (int i = 0; i < VALID_INTERVALS_COUNT; i++) {
    if (interval == VALID_INTERVALS[i]) {
      return true;
    }
  }
  return false;
}

bool validateRange(const String &range) {
  for (int i = 0; i < VALID_RANGES_COUNT; i++) {
    if (range == VALID_RANGES[i]) {
      return true;
    }
  }
  return false;
}

bool validateIP(const String &ip) {
  // Basic IP validation - check for 4 parts separated by dots
  int dotCount = 0;
  int partStart = 0;

  for (int i = 0; i <= ip.length(); i++) {
    if (i == ip.length() || ip.charAt(i) == '.') {
      if (i == partStart)
        return false; // Empty part

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
  Serial.println("\n=== calculateMaxBars() DEBUG ===");
  Serial.println("Input parameters:");
  Serial.println("  screenWidth: " + String(screenWidth));
  Serial.println("  panelWidth: " + String(panelWidth));
  Serial.println("  candleMinWidth: " + String(candleMinWidth));

  int chartWidth = screenWidth - panelWidth;
  Serial.println("  chartWidth: " + String(chartWidth));

  // Calculate maximum bars based on screen resolution
  int maxBarsScreen;
  int padding = CANDLE_PADDING;
  Serial.println("  CANDLE_PADDING: " + String(padding));

  if (padding == 0) {
    // Simple case: no padding, just divide available width by minimum candle
    // width
    maxBarsScreen = chartWidth / candleMinWidth;
  } else {
    // With padding: solve the equation
    maxBarsScreen = (chartWidth + padding) / (candleMinWidth + padding);
  }

  // UPDATED: Limit by data buffer size - can only show MAX_CANDLES - 5
  int maxBarsData = MAX_CANDLES - 5; // Reserve 5 slots for buffer management

  // Use the smaller of screen limitation or data limitation
  int actualMaxBars = std::min(maxBarsScreen, maxBarsData);

  // Reasonable bounds
  actualMaxBars = std::max(5, actualMaxBars);           // Minimum of 5 bars
  actualMaxBars = std::min(actualMaxBars, maxBarsData); // Cap at data limit

  Serial.println("Calculations:");
  Serial.println("  maxBarsScreen: " + String(maxBarsScreen));
  Serial.println("  maxBarsData: " + String(maxBarsData) +
                 " (MAX_CANDLES - 5)");
  Serial.println("  actualMaxBars: " + String(actualMaxBars));

  // Additional debug: show what the actual candle width would be
  int actual_candle_width;
  if (padding == 0) {
    actual_candle_width = chartWidth / actualMaxBars;
  } else {
    int total_padding_space = (actualMaxBars - 1) * padding;
    actual_candle_width = (chartWidth - total_padding_space) / actualMaxBars;
  }
  Serial.println("  actual_candle_width at max bars: " +
                 String(actual_candle_width));
  Serial.println("================================\n");

  int renderingBuffer = 5; // "minus 5" for reasons due to padding and rounding
  int bufferedMax = actualMaxBars - renderingBuffer;

  // Ensure we don't go below minimum
  bufferedMax = std::max(5, bufferedMax);

  Serial.println("Applied rendering buffer:");
  Serial.println("  theoretical_max: " + String(actualMaxBars));
  Serial.println("  rendering_buffer: " + String(renderingBuffer));
  Serial.println("  final_max: " + String(bufferedMax));

  return bufferedMax;
}

int getScreenWidth() {
  lv_disp_t *disp = lv_disp_get_default();
  if (disp != NULL) {
    return lv_disp_get_hor_res(disp);
  }
  return 320; // Fallback for T-Display-AMOLED
}

void syncCandleDurationWithInterval() {
  int intervalSeconds = 0;

  if (YAHOO_INTERVAL == "1m")
    intervalSeconds = 60;
  else if (YAHOO_INTERVAL == "2m")
    intervalSeconds = 120;
  else if (YAHOO_INTERVAL == "5m")
    intervalSeconds = 300;
  else if (YAHOO_INTERVAL == "15m")
    intervalSeconds = 900;
  else if (YAHOO_INTERVAL == "30m")
    intervalSeconds = 1800;
  else if (YAHOO_INTERVAL == "60m" || YAHOO_INTERVAL == "1h")
    intervalSeconds = 3600;
  else if (YAHOO_INTERVAL == "90m")
    intervalSeconds = 5400;
  else if (YAHOO_INTERVAL == "1d")
    intervalSeconds = 86400;
  else if (YAHOO_INTERVAL == "5d")
    intervalSeconds = 432000; // 5 days
  else if (YAHOO_INTERVAL == "1wk")
    intervalSeconds = 604800; // 1 week
  else if (YAHOO_INTERVAL == "1mo")
    intervalSeconds = 2592000; // 30 days (approx)
  else if (YAHOO_INTERVAL == "3mo")
    intervalSeconds = 7776000; // 90 days (approx)
  else {
    // For unsupported intervals, default to 5 minutes
    Serial.println("Warning: Unsupported interval, defaulting to 5 minutes");
    intervalSeconds = 300;
  }

  if (intervalSeconds != CANDLE_COLLECTION_DURATION) {
    Serial.printf("Auto-syncing candle duration: %d -> %d seconds (%s)\n",
                  CANDLE_COLLECTION_DURATION, intervalSeconds,
                  YAHOO_INTERVAL.c_str());
    CANDLE_COLLECTION_DURATION = intervalSeconds;
  }
}

bool setConfigFromJSON(const String &json) {
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
    int newInterval = doc["updateInterval"];
    Serial.println("JSON updateInterval received: " + String(newInterval) +
                   "ms");
    INTRADAY_UPDATE_INTERVAL = newInterval;
    Serial.println("INTRADAY_UPDATE_INTERVAL set to: " +
                   String(INTRADAY_UPDATE_INTERVAL) + "ms");
  }

  // NEW: Handle test data updates per bar
  if (doc["testUpdatesPerBar"].is<int>()) {
    int updatesPerBar = doc["testUpdatesPerBar"];
    if (updatesPerBar >= 1 && updatesPerBar <= 1000) {
      TEST_DATA_UPDATES_PER_BAR = updatesPerBar;
      Serial.println("Test updates per bar set to: " +
                     String(TEST_DATA_UPDATES_PER_BAR));
    } else {
      Serial.println("Invalid test updates per bar value, keeping current: " +
                     String(TEST_DATA_UPDATES_PER_BAR));
    }
  }

  if (doc["symbol"].is<String>()) {
    String symbol = doc["symbol"].as<String>();
    symbol.toUpperCase();
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
      syncCandleDurationWithInterval();
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
    int maxBarsAllowed =
        calculateMaxBars(getScreenWidth(), INFO_PANEL_WIDTH, 1);
    // maxBarsAllowed now already considers both screen and data limitations

    if (bars >= 1 && bars <= maxBarsAllowed) {
      BARS_TO_SHOW = bars;
      Serial.printf("Bars to show set to: %d (max allowed: %d)\n", BARS_TO_SHOW,
                    maxBarsAllowed);
    } else {
      // Clamp to valid range
      BARS_TO_SHOW = std::min(bars, maxBarsAllowed);
      BARS_TO_SHOW = std::max(BARS_TO_SHOW, 1);
      Serial.printf("Bars clamped from %d to %d (max allowed: %d, limited by "
                    "data buffer)\n",
                    bars, BARS_TO_SHOW, maxBarsAllowed);
    }
    printBarLimitations();
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
    if (validateIP(mask)) {
      SUBNET_MASK = mask;
    }
  }

  // Save the updated configuration
  saveConfig();
  return true;
}

String getConfigJSON() {
  JsonDocument doc;

  doc["useTestData"] = USE_TEST_DATA;
  doc["useIntraday"] = USE_INTRADAY_DATA;
  doc["updateInterval"] = INTRADAY_UPDATE_INTERVAL;
  doc["symbol"] = STOCK_SYMBOL;
  doc["enforceHours"] = ENFORCE_MARKET_HOURS;
  doc["yahooInterval"] = YAHOO_INTERVAL;
  doc["yahooRange"] = YAHOO_RANGE;
  doc["barsToShow"] = BARS_TO_SHOW;
  doc["testUpdatesPerBar"] = TEST_DATA_UPDATES_PER_BAR;

  // Add computed candle duration for display purposes (read-only)
  doc["computedCandleDuration"] = CANDLE_COLLECTION_DURATION;

  // Calculate maximum bars - this function now handles MAX_CANDLES - 5
  // internally
  int actualScreenWidth = getScreenWidth();
  int maxBarsAllowed = calculateMaxBars(actualScreenWidth, INFO_PANEL_WIDTH, 1);

  // Send the final allowed maximum and debug info
  doc["maxBars"] = maxBarsAllowed; // This is what the web UI should use
  doc["screenWidth"] = actualScreenWidth;

  // Optional debug info (you can keep or remove these)
  doc["maxBarsScreen"] =
      (actualScreenWidth - INFO_PANEL_WIDTH); // Raw screen capacity
  doc["maxBarsData"] = MAX_CANDLES - 5;       // Data buffer limitation

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

void printBarLimitations() {
  int screenWidth = getScreenWidth();
  int chartWidth = screenWidth - INFO_PANEL_WIDTH;
  int maxBarsScreen = chartWidth / 1; // Minimum 1-pixel candles
  int maxBarsData = MAX_CANDLES - 5;
  int actualMax = std::min(maxBarsScreen, maxBarsData);

  Serial.println("=== BAR DISPLAY LIMITATIONS ===");
  Serial.println("Screen width: " + String(screenWidth));
  Serial.println("Chart width: " + String(chartWidth));
  Serial.println("Theoretical max (1px candles): " + String(maxBarsScreen));
  Serial.println("Data buffer limit: " + String(maxBarsData) +
                 " (MAX_CANDLES - 5)");
  Serial.println("Actual maximum bars: " + String(actualMax));
  Serial.println("Current BARS_TO_SHOW: " + String(BARS_TO_SHOW));
  Serial.println("Limiting factor: " + String(actualMax == maxBarsScreen
                                                  ? "Screen resolution"
                                                  : "Data buffer"));
  Serial.println("===============================");
}