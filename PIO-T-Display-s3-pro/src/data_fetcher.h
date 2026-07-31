#ifndef DATA_FETCHER_H
#define DATA_FETCHER_H

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>
#include <vector>

typedef struct {
  float open;
  float close;
  float high;
  float low;
  uint64_t volume;
  time_t timestamp;
  bool is_complete; // Flag to indicate if candle is complete
} enhanced_candle_t;

// Fetches OHLC bars from the Yahoo Finance chart API.
//
// Ported from CandleCollector: requests are windowed (period1/period2) and
// sized so the response can never exceed what the ring buffer holds, and the
// body is streamed straight into a filtered JSON parse instead of being
// buffered into a String. The previous implementation pulled a whole
// `range=` payload into RAM and gave up above 50KB - which a 5m/1mo request
// exceeds by 5x, silently degrading the chart to a single daily candle.
//
// Live updates fetch only the trailing few intervals and merge the returned
// bars into the buffer by timestamp, so candles carry Yahoo's real OHLC
// instead of an approximation built from sampled last-prices.
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
  static unsigned long consecutive_failures;
  static time_t last_success_time;

  // Helper methods
  static void updateCircularBuffer(const enhanced_candle_t &candle);
  static float getRandomPrice(); // For test data
  static bool shouldCreateNewCandle(time_t current_time,
                                    time_t last_candle_time,
                                    int interval_seconds);
  static void buildIntradayCandle(float price, time_t timestamp);

  // Streams one windowed chart request into `out`, oldest bar first. Returns
  // false on a network/parse error; an empty result with true means the
  // window contained no trading. The bar at exactly period1 is dropped
  // (Yahoo reports it with degraded fields) and the in-progress bar is kept
  // only when `keepPartial` is set.
  static bool fetchWindow(const String &symbol, const String &interval,
                          time_t period1, time_t period2, bool keepPartial,
                          std::vector<enhanced_candle_t> &out);

  // Inserts a bar, replacing the entry with the same timestamp if present
  // (late revisions, in-progress candle) and appending when it is newer.
  // `intervalSec` is the series' bar length: an append must clear it, which
  // is what keeps Yahoo's moving regularMarketTime entry from landing as a
  // new candle. Returns true only if the buffer changed.
  static bool upsertCandle(const enhanced_candle_t &candle, int intervalSec);

  // Calendar seconds to request so the response holds at most `maxBars`
  // bars, clamped to Yahoo's per-interval history limit.
  static long lookbackSeconds(const String &interval, const String &range,
                              int maxBars);

public:
  static bool initialize(const String &symbol);
  static bool updateData();
  static bool fetchInitialData(const String &symbol, const String &interval,
                               const String &range);
  static enhanced_candle_t *getCandles() { return candles; }
  static int getCandleCount() { return num_candles; }
  static int getNewestIndex() { return newest_candle_index; }
  static float getCurrentPrice() { return current_price; }
  static bool isLoaded() { return initial_data_loaded; }
  static unsigned long getConsecutiveFailures() { return consecutive_failures; }
  static time_t getLastSuccessTime() { return last_success_time; }
  static void getPriceLevels(float *min_price, float *max_price);
  static void getPriceLevelsForVisibleBars(float *min_price, float *max_price,
                                           int bars_to_show);
  static void initializeTestData();
  static bool validateCandle(const enhanced_candle_t &candle);
  static void reset();

  // Seconds per bar for a Yahoo interval string ("5m" -> 300).
  static int getIntervalSeconds(const String &interval);
};

#endif // DATA_FETCHER_H
