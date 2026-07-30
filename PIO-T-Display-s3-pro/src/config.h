#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Full POSIX TZ spec for the device's own locale (US Mountain). newlib needs
// explicit DST rules; a bare "MST7MDT" applies standard time year-round,
// which puts the on-screen clock and every market-hours comparison an hour
// off for eight months of the year.
#define TIME_ZONE "MST7MDT,M3.2.0/2,M11.1.0"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"

#define MAX_CANDLES 500 // Define this as a constant
#define INFO_PANEL_WIDTH 80
#define CANDLE_PADDING 0

// Fallback access point + mDNS/OTA identity. The AP is raised whenever no
// saved network is reachable, so the device is always onboardable; the same
// password authenticates over-the-air updates.
#define AP_SSID "LilyGoTicker"
#define AP_PASS "ticker1234"
#define MDNS_HOST "lilygoticker"

// Floor on how often real market data is fetched. Each fetch is a TLS
// handshake plus a blocking parse, so anything faster starves the LVGL task
// without showing more price action.
#define MIN_REAL_UPDATE_INTERVAL_MS 5000

// Stock data configuration
extern bool USE_TEST_DATA;
extern bool USE_INTRADAY_DATA;
extern int INTRADAY_UPDATE_INTERVAL;
extern int CANDLE_COLLECTION_DURATION;
extern String STOCK_SYMBOL;
extern bool ENFORCE_MARKET_HOURS;
extern int
    TEST_DATA_UPDATES_PER_BAR; // Number of updates to form one bar in test mode

// New Yahoo Finance API parameters
extern String
    YAHOO_INTERVAL; // 1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo
extern String YAHOO_RANGE; // 1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, ytd, max

// Chart display configuration
extern int BARS_TO_SHOW; // Added missing declaration

// Valid options for dropdowns (symbols removed - now free text input)
extern const char *VALID_INTERVALS[];
extern const int VALID_INTERVALS_COUNT;
extern const char *VALID_RANGES[];
extern const int VALID_RANGES_COUNT;

// Network configuration
extern bool USE_STATIC_IP;
extern String STATIC_IP;
extern String GATEWAY_IP;
extern String SUBNET_MASK;

// Function to load configuration
void loadConfig();
void saveConfig();
void syncCandleDurationWithInterval(); // NEW: Auto-sync function
bool validateInterval(const String &interval);
bool validateRange(const String &range);
bool validateSymbol(const String &symbol);
bool validateIP(const String &ip);
int calculateMaxBars(int screenWidth, int panelWidth = 80,
                     int candleMinWidth = 1);
int getScreenWidth();
String getConfigJSON();
bool setConfigFromJSON(const String &json);
void printBarLimitations();

#endif // CONFIG_H