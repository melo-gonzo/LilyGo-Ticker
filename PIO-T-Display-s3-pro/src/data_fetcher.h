#ifndef DATA_FETCHER_H
#define DATA_FETCHER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"

typedef struct {
    float open;
    float close;
    float high;
    float low;
    time_t timestamp;
    bool is_complete;  // Flag to indicate if candle is complete
} enhanced_candle_t;

class DataFetcher {
private:
    static enhanced_candle_t candles[MAX_CANDLES];
    static int newest_candle_index;
    static int num_candles;
    static time_t last_update_time;
    static float current_price;
    static bool initial_data_loaded;
    static String current_symbol;
    static String current_interval;
    static String current_range;
    
    // Helper methods
    static bool fetchYahooData(const String& symbol, const String& interval, const String& range);
    static void updateCircularBuffer(const enhanced_candle_t& candle);
    static float getRandomPrice(); // For test data
    static int getIntervalSeconds(const String& interval);
    static bool shouldCreateNewCandle(time_t current_time, time_t last_candle_time, int interval_seconds);
    static void buildIntradayCandle(float price, time_t timestamp);
    static bool isDataStale();
    static String getSmallerRange(const String& range);
    static bool fetchFallbackData(const String& symbol);
    
public:
    static bool initialize(const String& symbol);
    static bool updateData();
    static bool fetchInitialData(const String& symbol, const String& interval, const String& range);
    static enhanced_candle_t* getCandles() { return candles; }
    static int getCandleCount() { return num_candles; }
    static int getNewestIndex() { return newest_candle_index; }
    static float getCurrentPrice() { return current_price; }
    static void getPriceLevels(float* min_price, float* max_price);
    static void getPriceLevelsForVisibleBars(float* min_price, float* max_price, int bars_to_show); // NEW METHOD
    static void initializeTestData();
    static void reset();
};

#endif // DATA_FETCHER_H