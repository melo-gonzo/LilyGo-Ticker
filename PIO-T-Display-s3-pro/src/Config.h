#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define TIME_ZONE "PST8PDT" // Set to the desired time zone
#define MAX_CANDLES 130  // Define this as a constant
#define INFO_PANEL_WIDTH 80
#define CANDLE_PADDING 0

// Stock data configuration
extern bool USE_TEST_DATA;
extern bool USE_INTRADAY_DATA;
extern int INTRADAY_UPDATE_INTERVAL;
extern int CANDLE_COLLECTION_DURATION;
extern String STOCK_SYMBOL;
extern bool ENFORCE_MARKET_HOURS;

// New Yahoo Finance API parameters
extern String YAHOO_INTERVAL;  // 1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo
extern String YAHOO_RANGE;     // 1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, ytd, max

// Chart display configuration
extern int BARS_TO_SHOW;  // Added missing declaration

// Valid options for dropdowns
extern const char* VALID_INTERVALS[];
extern const int VALID_INTERVALS_COUNT;
extern const char* VALID_RANGES[];
extern const int VALID_RANGES_COUNT;
extern const char* VALID_SYMBOLS[];
extern const int VALID_SYMBOLS_COUNT;

// Network configuration
extern bool USE_STATIC_IP;
extern String STATIC_IP;
extern String GATEWAY_IP;
extern String SUBNET_MASK;

// Function to load configuration
void loadConfig();
void saveConfig();
bool validateInterval(const String& interval);
bool validateRange(const String& range);
bool validateIP(const String& ip);
int calculateMaxBars(int screenWidth, int panelWidth = 80, int candleMinWidth = 1);
int getScreenWidth();
String getConfigJSON();
bool setConfigFromJSON(const String& json);

#endif // CONFIG_H