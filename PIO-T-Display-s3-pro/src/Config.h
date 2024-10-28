#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define TIME_ZONE "PST8PDT" // Set to the desired time zone

#define MAX_CANDLES 20  // Define this as a constant

// Stock data configuration
extern bool USE_TEST_DATA;
extern bool USE_INTRADAY_DATA;
extern int INTRADAY_UPDATE_INTERVAL;
extern int CANDLE_COLLECTION_DURATION;
extern const char* STOCK_SYMBOL;

// Function to load configuration
void loadConfig();

#endif // CONFIG_H
