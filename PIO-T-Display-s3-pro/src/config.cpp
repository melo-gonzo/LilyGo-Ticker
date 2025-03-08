#include "config.h"


// Intraday with real data
bool USE_TEST_DATA = false;
bool USE_INTRADAY_DATA = true;
int INTRADAY_UPDATE_INTERVAL = 5; // 5
int CANDLE_COLLECTION_DURATION = 180; // 60
const char* STOCK_SYMBOL = "SPY";

// Test intraday with fake data
// bool USE_TEST_DATA = true;
// bool USE_INTRADAY_DATA = true;
// int INTRADAY_UPDATE_INTERVAL = 1; // 5 // 1 
// int CANDLE_COLLECTION_DURATION = 6; // 60 // 6
// const char* STOCK_SYMBOL = "SPY";

// Daily data
// bool USE_TEST_DATA = false;
// bool USE_INTRADAY_DATA = false;
// int INTRADAY_UPDATE_INTERVAL = 5; // 5
// int CANDLE_COLLECTION_DURATION = 60; // 60
// const char* STOCK_SYMBOL = "SPY";


void loadConfig() {
    // In the future, you could load these values from EEPROM or a file system
    // For now, we'll just use the default values
    // Example of how you might load from EEPROM:
    // USE_TEST_DATA = EEPROM.read(0);
    // USE_INTRADAY_DATA = EEPROM.read(1);
    // EEPROM.get(2, INTRADAY_UPDATE_INTERVAL);
    // ...
}