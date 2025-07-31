#include "data_fetcher.h"
#include "market_hours.h"
#include <algorithm>

// Static member definitions
enhanced_candle_t DataFetcher::candles[MAX_CANDLES];
int DataFetcher::newest_candle_index = -1;
int DataFetcher::num_candles = 0;
time_t DataFetcher::last_update_time = 0;
float DataFetcher::current_price = 0.0;
bool DataFetcher::initial_data_loaded = false;
String DataFetcher::current_symbol = "";
String DataFetcher::current_interval = "";
String DataFetcher::current_range = "";

bool DataFetcher::initialize(const String &symbol) {
  // Always reset first to ensure clean state
  reset();

  current_symbol = symbol;
  current_interval = YAHOO_INTERVAL;
  current_range = YAHOO_RANGE;

  Serial.println("Initializing DataFetcher for symbol: " + symbol);
  Serial.println("Use test data: " + String(USE_TEST_DATA));

  if (USE_TEST_DATA) {
    Serial.println("Using test data mode");
    initializeTestData();
    return true;
  } else {
    Serial.println("Using real data mode");
    // Fetch initial historical data
    return fetchInitialData(symbol, YAHOO_INTERVAL, YAHOO_RANGE);
  }
}

bool DataFetcher::fetchInitialData(const String &symbol, const String &interval,
                                   const String &range) {
  Serial.println("Fetching initial data for " + symbol +
                 " with interval=" + interval + " range=" + range);

  HTTPClient http;
  String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + symbol +
               "?interval=" + interval + "&range=" + range;

  Serial.println("URL: " + url);
  http.begin(url);
  http.setTimeout(10000); // 10 second timeout

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("HTTP request failed with code: " + String(httpCode));
    http.end();
    return false;
  }

  String payload = http.getString();
  int payloadSize = payload.length();
  Serial.println("Response size: " + String(payloadSize) + " bytes");

  http.end();

  // Check if payload is too large for ESP32 to handle
  if (payloadSize > 50000) { // 50KB limit
    Serial.println("Response too large (" + String(payloadSize) +
                   " bytes), trying smaller range...");

    // Try with a smaller range
    String smallerRange = getSmallerRange(range);
    if (smallerRange != range) {
      Serial.println("Retrying with smaller range: " + smallerRange);
      return fetchInitialData(symbol, interval, smallerRange);
    } else {
      Serial.println("Cannot reduce range further, using fallback");
      return fetchFallbackData(symbol);
    }
  }

  // Use standard JsonDocument with automatic sizing
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("JSON parsing failed: " + String(error.c_str()));
    Serial.println("Payload size was: " + String(payloadSize));

    // Try with fallback data if JSON parsing fails
    return fetchFallbackData(symbol);
  }

  // Check if we have valid data
  if (!doc["chart"]["result"][0]["timestamp"]) {
    Serial.println("No timestamp data in response");
    return false;
  }

  JsonArray timestamps = doc["chart"]["result"][0]["timestamp"].as<JsonArray>();
  JsonObject quote = doc["chart"]["result"][0]["indicators"]["quote"][0];
  JsonArray opens = quote["open"].as<JsonArray>();
  JsonArray highs = quote["high"].as<JsonArray>();
  JsonArray lows = quote["low"].as<JsonArray>();
  JsonArray closes = quote["close"].as<JsonArray>();

  if (timestamps.size() == 0) {
    Serial.println("No data points received");
    return false;
  }

  // Reset the buffer
  reset();

  // Load data into circular buffer
  int dataSize = std::min((int)timestamps.size(), MAX_CANDLES);
  int startIndex =
      timestamps.size() - dataSize; // Start from the most recent data that fits

  for (int i = 0; i < dataSize; i++) {
    int dataIndex = startIndex + i;

    enhanced_candle_t candle;
    candle.timestamp = timestamps[dataIndex].as<long>();
    candle.open = opens[dataIndex].as<float>();
    candle.high = highs[dataIndex].as<float>();
    candle.low = lows[dataIndex].as<float>();
    candle.close = closes[dataIndex].as<float>();
    candle.is_complete = true; // Historical data is always complete

    updateCircularBuffer(candle);
  }

  current_price = candles[newest_candle_index].close;
  initial_data_loaded = true;

  Serial.println("Loaded " + String(num_candles) +
                 " candles. Current price: " + String(current_price));
  return true;
}

String DataFetcher::getSmallerRange(const String &range) {
  // Return a smaller range to reduce data size
  if (range == "1y")
    return "6mo";
  if (range == "6mo")
    return "3mo";
  if (range == "3mo")
    return "1mo";
  if (range == "1mo")
    return "5d";
  if (range == "5d")
    return "1d";
  return range; // Can't reduce further
}

bool DataFetcher::fetchFallbackData(const String &symbol) {
  Serial.println("Using fallback: fetching 1d data with daily interval");

  HTTPClient http;
  String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + symbol +
               "?interval=1d&range=1d";

  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Fallback request failed");
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("Fallback JSON parsing failed");
    return false;
  }

  // Process fallback data (simplified)
  JsonArray timestamps = doc["chart"]["result"][0]["timestamp"].as<JsonArray>();
  JsonObject quote = doc["chart"]["result"][0]["indicators"]["quote"][0];
  JsonArray closes = quote["close"].as<JsonArray>();

  if (timestamps.size() == 0) {
    return false;
  }

  // Create a single candle from the most recent data
  reset();
  enhanced_candle_t candle;
  int lastIndex = timestamps.size() - 1;
  candle.timestamp = timestamps[lastIndex].as<long>();
  candle.open = quote["open"][lastIndex].as<float>();
  candle.high = quote["high"][lastIndex].as<float>();
  candle.low = quote["low"][lastIndex].as<float>();
  candle.close = closes[lastIndex].as<float>();
  candle.is_complete = true;

  updateCircularBuffer(candle);
  current_price = candle.close;
  initial_data_loaded = true;

  Serial.println("Fallback data loaded: 1 candle, price: " +
                 String(current_price));
  return true;
}

bool DataFetcher::updateData() {
  // Use different timing mechanisms for test vs real data
  static unsigned long last_update_millis =
      0; // For test data (millisecond precision)

  if (USE_TEST_DATA) {
    // TEST DATA MODE: Use millis() for precise millisecond timing
    if (millis() - last_update_millis < INTRADAY_UPDATE_INTERVAL) {
      return false;
    }

    last_update_millis = millis();

    // Generate new test data point
    time_t now;
    time(&now);
    float price = getRandomPrice();

    // Track candle state before update
    int candles_before = num_candles;
    bool was_complete =
        (num_candles > 0) ? candles[newest_candle_index].is_complete : false;

    buildIntradayCandle(price, now);

    // Track candle state after update
    int candles_after = num_candles;
    bool is_complete =
        (num_candles > 0) ? candles[newest_candle_index].is_complete : false;

    // Enhanced debug output every few seconds
    static unsigned long lastTestDebug = 0;
    if (millis() - lastTestDebug > 3000) { // Every 3 seconds instead of 5
      Serial.println("\n=== TEST DATA STATUS ===");
      Serial.println("Update interval: " + String(INTRADAY_UPDATE_INTERVAL) +
                     "ms");
      Serial.println("Updates per bar: " + String(TEST_DATA_UPDATES_PER_BAR));
      Serial.println("Current price: " + String(price));
      Serial.println("Total candles: " + String(num_candles));

      if (num_candles > 0) {
        Serial.println("Current candle status: " +
                       String(is_complete ? "COMPLETE" : "BUILDING"));
        Serial.println("Current candle OHLC: O:" +
                       String(candles[newest_candle_index].open) +
                       " H:" + String(candles[newest_candle_index].high) +
                       " L:" + String(candles[newest_candle_index].low) +
                       " C:" + String(candles[newest_candle_index].close));
      }

      // Show when candles complete or new ones start
      if (candles_after > candles_before) {
        Serial.println(">>> NEW CANDLE CREATED <<<");
      }
      if (!was_complete && is_complete) {
        Serial.println(">>> CANDLE COMPLETED <<<");
      }

      Serial.println("========================\n");
      lastTestDebug = millis();
    }

    return true;
  }

  // REAL DATA MODE: Use time() but convert interval to seconds
  time_t now;
  time(&now);

  // Convert milliseconds to seconds for real data timing comparison
  int update_interval_seconds = INTRADAY_UPDATE_INTERVAL / 1000;
  if (update_interval_seconds < 1) {
    update_interval_seconds = 1; // Minimum 1 second for real data
  }

  // Check if we need to update
  if (now - last_update_time < update_interval_seconds) {
    return false;
  }

  last_update_time = now;

  // Check market hours if enforced (for real data only)
  if (ENFORCE_MARKET_HOURS &&
      !StockTracker::MarketHoursChecker::isMarketOpen()) {
    Serial.println("Market is closed, skipping real data update");
    return false;
  }

  // Fetch current price for intraday candle building (real data)
  HTTPClient http;
  String url = "https://query1.finance.yahoo.com/v8/finance/chart/" +
               current_symbol + "?interval=1m&range=1d";

  Serial.println("Fetching real data from: " + url);
  http.begin(url);
  http.setTimeout(10000); // 10 second timeout
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("HTTP request failed with code: " + String(httpCode));
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("Empty response received");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("JSON parsing failed: " + String(error.c_str()));
    return false;
  }

  // Check if we have valid response structure
  if (!doc["chart"]["result"][0]["timestamp"]) {
    Serial.println("Invalid response structure - no timestamp data");
    return false;
  }

  JsonArray timestamps = doc["chart"]["result"][0]["timestamp"].as<JsonArray>();
  JsonObject quote = doc["chart"]["result"][0]["indicators"]["quote"][0];
  JsonArray closes = quote["close"].as<JsonArray>();

  if (timestamps.size() == 0 || closes.size() == 0) {
    Serial.println("No price data in response");
    return false;
  }

  // Get the most recent price
  float latestPrice = 0.0;
  time_t latestTimestamp = 0;

  // Search backwards through the arrays to find the most recent non-null price
  for (int i = closes.size() - 1; i >= 0; i--) {
    if (!closes[i].isNull() && closes[i].as<float>() > 0) {
      latestPrice = closes[i].as<float>();
      latestTimestamp = timestamps[i].as<long>();
      break;
    }
  }

  if (latestPrice > 0) {
    Serial.println("Real data update - Latest price: " + String(latestPrice) +
                   " at timestamp: " + String(latestTimestamp));

    current_price = latestPrice;
    buildIntradayCandle(latestPrice, latestTimestamp);
    return true;
  } else {
    Serial.println("No valid price found in response");
    return false;
  }
}

void DataFetcher::buildIntradayCandle(float price, time_t timestamp) {
  static int update_count = 0;
  static int last_candle_count = 0; // Track if data was reset

  // Reset update counter if data was cleared
  if (num_candles < last_candle_count || num_candles == 0) {
    update_count = 0;
    Serial.println("Test data: Update counter reset due to data clear");
  }
  last_candle_count = num_candles;

  if (USE_TEST_DATA) {
    // TEST DATA MODE: Use update counting
    update_count++;

    if (num_candles == 0) {
      // Create the very first candle
      enhanced_candle_t newCandle;
      newCandle.timestamp = timestamp;
      newCandle.open = price;
      newCandle.high = price;
      newCandle.low = price;
      newCandle.close = price;
      newCandle.is_complete = false; // Incomplete until we reach update limit

      updateCircularBuffer(newCandle);
      Serial.println("Test data: Created first candle - Update 1/" +
                     String(TEST_DATA_UPDATES_PER_BAR));
    } else {
      // Check if current candle is complete and we need a new one
      if (candles[newest_candle_index].is_complete) {
        // Previous candle was completed, start a new one
        enhanced_candle_t newCandle;
        newCandle.timestamp = timestamp;
        newCandle.open = price;
        newCandle.high = price;
        newCandle.low = price;
        newCandle.close = price;
        newCandle.is_complete = false; // Start as incomplete

        updateCircularBuffer(newCandle);
        update_count = 1; // Reset counter for new candle
        Serial.println("Test data: Started new candle - Update 1/" +
                       String(TEST_DATA_UPDATES_PER_BAR) +
                       " (Price: " + String(price) + ")");
      } else {
        // Update the current incomplete candle
        candles[newest_candle_index].close = price;
        candles[newest_candle_index].high =
            std::max(candles[newest_candle_index].high, price);
        candles[newest_candle_index].low =
            std::min(candles[newest_candle_index].low, price);
        candles[newest_candle_index].timestamp = timestamp;

        // Check if we should complete this candle
        if (update_count >= TEST_DATA_UPDATES_PER_BAR) {
          candles[newest_candle_index].is_complete = true;
          Serial.println(
              "Test data: Completed candle after " + String(update_count) +
              " updates" + " (Final price: " + String(price) +
              ", Open: " + String(candles[newest_candle_index].open) +
              ", High: " + String(candles[newest_candle_index].high) +
              ", Low: " + String(candles[newest_candle_index].low) + ")");
          // Note: Don't reset update_count here - let it reset when new candle
          // starts
        } else {
          Serial.println(
              "Test data: Update " + String(update_count) + "/" +
              String(TEST_DATA_UPDATES_PER_BAR) + " - Price: " + String(price) +
              " (Range: " + String(candles[newest_candle_index].low) + "-" +
              String(candles[newest_candle_index].high) + ")");
        }
      }
    }

    current_price = price;
    return;
  }

  // REAL DATA MODE: Use existing time-based logic (unchanged)
  int interval_seconds = getIntervalSeconds(YAHOO_INTERVAL);

  if (num_candles == 0 ||
      shouldCreateNewCandle(timestamp, candles[newest_candle_index].timestamp,
                            interval_seconds)) {
    enhanced_candle_t newCandle;
    newCandle.timestamp = timestamp;
    newCandle.open = price;
    newCandle.high = price;
    newCandle.low = price;
    newCandle.close = price;
    newCandle.is_complete = true;

    updateCircularBuffer(newCandle);
  } else {
    candles[newest_candle_index].close = price;
    candles[newest_candle_index].high =
        std::max(candles[newest_candle_index].high, price);
    candles[newest_candle_index].low =
        std::min(candles[newest_candle_index].low, price);
    candles[newest_candle_index].is_complete = true;
  }

  current_price = price;
}

bool DataFetcher::shouldCreateNewCandle(time_t current_time,
                                        time_t last_candle_time,
                                        int interval_seconds) {
  return (current_time / interval_seconds) !=
         (last_candle_time / interval_seconds);
}

int DataFetcher::getIntervalSeconds(const String &interval) {
  if (interval == "1m")
    return 60;
  if (interval == "2m")
    return 120;
  if (interval == "5m")
    return 300;
  if (interval == "15m")
    return 900;
  if (interval == "30m")
    return 1800;
  if (interval == "60m" || interval == "1h")
    return 3600;
  if (interval == "90m")
    return 5400;
  if (interval == "1d")
    return 86400;

  // Default to the configured candle collection duration
  return CANDLE_COLLECTION_DURATION;
}

void DataFetcher::updateCircularBuffer(const enhanced_candle_t &candle) {
  if (num_candles < MAX_CANDLES) {
    num_candles++;
  }

  newest_candle_index = (newest_candle_index + 1) % MAX_CANDLES;
  candles[newest_candle_index] = candle;
}

void DataFetcher::getPriceLevels(float *min_price, float *max_price) {
  *min_price = std::numeric_limits<float>::max();
  *max_price = std::numeric_limits<float>::lowest();

  if (num_candles == 0) {
    *min_price = 0;
    *max_price = 100;
    return;
  }

  for (int i = 0; i < num_candles; i++) {
    int index =
        (newest_candle_index - num_candles + i + 1 + MAX_CANDLES) % MAX_CANDLES;
    *min_price = std::min(*min_price, candles[index].low);
    *max_price = std::max(*max_price, candles[index].high);
  }

  // Round to 2 decimal places
  *min_price = floor(*min_price * 100) / 100;
  *max_price = ceil(*max_price * 100) / 100;
}

void DataFetcher::getPriceLevelsForVisibleBars(float *min_price,
                                               float *max_price,
                                               int bars_to_show) {
  *min_price = std::numeric_limits<float>::max();
  *max_price = std::numeric_limits<float>::lowest();

  if (num_candles == 0) {
    *min_price = 0;
    *max_price = 100;
    return;
  }

  // Limit bars_to_show to available data
  int barsToCheck = std::min(bars_to_show, num_candles);

  Serial.printf("Price calculation debug:\n");
  Serial.printf("  Requested bars: %d\n", bars_to_show);
  Serial.printf("  Available candles: %d\n", num_candles);
  Serial.printf("  Bars to check: %d\n", barsToCheck);
  Serial.printf("  Newest index: %d\n", newest_candle_index);

  // Calculate min/max for only the MOST RECENT visible bars
  // We want the newest barsToCheck candles, not the oldest ones
  for (int i = 0; i < barsToCheck; i++) {
    // Calculate index for the i-th most recent candle
    // newest_candle_index is the most recent (i=0)
    // newest_candle_index-1 is the second most recent (i=1), etc.
    int index = (newest_candle_index - i + MAX_CANDLES) % MAX_CANDLES;

    const enhanced_candle_t &candle = candles[index];

    // Debug output for first few candles
    if (i < 5) {
      Serial.printf(
          "  Bar %d: index=%d, high=%.2f, low=%.2f, open=%.2f, close=%.2f\n", i,
          index, candle.high, candle.low, candle.open, candle.close);
    }

    // Validate that we have real data (not uninitialized)
    if (candle.high > 0 && candle.low > 0 && candle.open > 0 &&
        candle.close > 0) {
      *min_price = std::min(*min_price, candle.low);
      *max_price = std::max(*max_price, candle.high);
    } else {
      Serial.printf(
          "  Warning: Invalid candle data at index %d (high=%.2f, low=%.2f)\n",
          index, candle.high, candle.low);
    }
  }

  // If we didn't find any valid data, use fallback
  if (*min_price == std::numeric_limits<float>::max()) {
    Serial.println("  No valid price data found, using fallback");
    *min_price = 0;
    *max_price = 100;
    return;
  }

  // Round to 2 decimal places
  *min_price = floor(*min_price * 100) / 100;
  *max_price = ceil(*max_price * 100) / 100;

  Serial.printf("  Final range: %.2f - %.2f\n", *min_price, *max_price);
}

float DataFetcher::getRandomPrice() {
  static float lastPrice = 250.0;
  static bool priceInitialized = false;
  static int lastCandleCount = 0; // Track if data was reset

  // Reset price initialization if data was cleared
  if (num_candles < lastCandleCount) {
    priceInitialized = false;
    lastPrice = 250.0;
    Serial.println("Test data: Price generator reset due to data clear");
  }
  lastCandleCount = num_candles;

  // If we have candles in the buffer, use the most recent close price
  if (!priceInitialized && num_candles > 0) {
    lastPrice = candles[newest_candle_index].close;
    priceInitialized = true;
    Serial.println(
        "Test data: Initialized price continuation from last candle: " +
        String(lastPrice));
  }

  // Generate realistic price movement (-1% to +1% change)
  float changePercent = (random(-100, 101) / 10000.0); // -0.01 to +0.01
  float change = lastPrice * changePercent;
  lastPrice += change;

  // Ensure price doesn't go negative
  lastPrice = std::max(0.01f, lastPrice);

  return lastPrice;
}

void DataFetcher::initializeTestData() {
  Serial.println("Initializing realistic test data...");
  Serial.println("Using " + String(TEST_DATA_UPDATES_PER_BAR) +
                 " updates per candle for realistic OHLC generation");

  float base_price = 250.0;
  time_t now;
  time(&now);

  // IMPORTANT: Reset the buffer first to clear any existing real data
  reset();
  Serial.println("Cleared existing data for test mode");

  // Pre-populate with historical test data
  int test_candles =
      std::min(MAX_CANDLES - 5, 500); // Generate up to 500 test candles

  for (int i = 0; i < test_candles; i++) {
    enhanced_candle_t candle;

    // Initialize the candle with starting price
    float starting_price = base_price;
    candle.open = starting_price;
    candle.high = starting_price;
    candle.low = starting_price;
    candle.close = starting_price;

    // Simulate TEST_DATA_UPDATES_PER_BAR price updates to build this candle
    float current_price = starting_price;

    // Serial.printf("Generating candle %d starting at %.2f with %d updates:\n",
    //              i, starting_price, TEST_DATA_UPDATES_PER_BAR);

    for (int update = 0; update < TEST_DATA_UPDATES_PER_BAR; update++) {
      // Generate realistic price movement for each update
      // Use smaller movements for individual updates (0.1% to 0.5% per update)
      float change_percent = (random(-50, 51) / 10000.0); // -0.5% to +0.5%
      float price_change = current_price * change_percent;
      current_price += price_change;

      // Ensure price doesn't go negative
      current_price = std::max(0.01f, current_price);

      // Update the candle's OHLC based on this price movement
      candle.high = std::max(candle.high, current_price);
      candle.low = std::min(candle.low, current_price);
      candle.close = current_price; // Close is always the last price

      // Debug output for first few updates of first few candles
      // if (i < 3 && update < 5) {
      //     Serial.printf("  Update %d: %.2f (change: %+.3f)\n",
      //                  update + 1, current_price, price_change);
      // } else if (i < 3 && update == TEST_DATA_UPDATES_PER_BAR - 1) {
      //     Serial.printf("  Update %d (final): %.2f\n", update + 1,
      //     current_price);
      // }
    }

    // Ensure low is never higher than open/close and high is never lower
    candle.low = std::min({candle.low, candle.open, candle.close});
    candle.high = std::max({candle.high, candle.open, candle.close});

    // Set timestamp going backwards in time
    candle.timestamp = now - (test_candles - i) * CANDLE_COLLECTION_DURATION;
    candle.is_complete = true; // Mark all historical test data as complete

    updateCircularBuffer(candle);

    // Use the close price as the base for the next candle (price continuity)
    base_price = candle.close;

    // Summary for first few candles
    if (i < 5) {
      float price_move = candle.close - candle.open;
      float price_range = candle.high - candle.low;
      Serial.printf("Candle %d complete: O:%.2f H:%.2f L:%.2f C:%.2f (Move: "
                    "%+.2f, Range: %.2f)\n",
                    i, candle.open, candle.high, candle.low, candle.close,
                    price_move, price_range);
    }
  }

  current_price = candles[newest_candle_index].close;
  initial_data_loaded = true;

  Serial.println("\n=== Test Data Generation Complete ===");
  Serial.println("Generated " + String(num_candles) + " realistic candles");
  Serial.println("Each candle built from " + String(TEST_DATA_UPDATES_PER_BAR) +
                 " price updates");
  Serial.println("Final price for live continuation: " + String(current_price));

  // Calculate and display statistics
  float total_range = 0;
  float total_movement = 0;
  int up_candles = 0, down_candles = 0;

  for (int i = 0; i < std::min(num_candles, 50); i++) { // Check last 50 candles
    int idx = (newest_candle_index - i + MAX_CANDLES) % MAX_CANDLES;
    const enhanced_candle_t &c = candles[idx];

    total_range += (c.high - c.low);
    float movement = c.close - c.open;
    total_movement += abs(movement);

    if (movement > 0)
      up_candles++;
    else if (movement < 0)
      down_candles++;
  }

  int sample_size = std::min(num_candles, 50);
  Serial.println("Statistics for last " + String(sample_size) + " candles:");
  Serial.printf("  Average range: %.2f\n", total_range / sample_size);
  Serial.printf("  Average movement: %.2f\n", total_movement / sample_size);
  Serial.printf("  Up candles: %d, Down candles: %d, Doji: %d\n", up_candles,
                down_candles, sample_size - up_candles - down_candles);

  // Print some sample candles for verification - FIXED: Use ASCII characters
  Serial.println("\nSample candles (most recent):");
  for (int i = 0; i < std::min(5, num_candles); i++) {
    int idx = (newest_candle_index - i + MAX_CANDLES) % MAX_CANDLES;
    const enhanced_candle_t &c = candles[idx];
    float move = c.close - c.open;
    float range = c.high - c.low;

    // FIXED: Use simple ASCII characters instead of Unicode
    char direction = (move > 0) ? '^' : (move < 0) ? 'v' : '-';

    // Serial.printf("  Candle -%d: O:%.2f H:%.2f L:%.2f C:%.2f %c (Move:%+.2f
    // Range:%.2f)\n",
    //              i, c.open, c.high, c.low, c.close, direction, move, range);
  }

  Serial.println("Ready for live test data updates...\n");
}

// Also add the validation function without Unicode characters:
bool DataFetcher::validateCandle(const enhanced_candle_t &candle) {
  // Ensure OHLC relationships are valid
  if (candle.high < candle.open || candle.high < candle.close ||
      candle.high < candle.low) {
    Serial.printf("ERROR: Invalid high %.2f vs O:%.2f H:%.2f L:%.2f C:%.2f\n",
                  candle.high, candle.open, candle.high, candle.low,
                  candle.close);
    return false;
  }

  if (candle.low > candle.open || candle.low > candle.close ||
      candle.low > candle.high) {
    Serial.printf("ERROR: Invalid low %.2f vs O:%.2f H:%.2f L:%.2f C:%.2f\n",
                  candle.low, candle.open, candle.high, candle.low,
                  candle.close);
    return false;
  }

  if (candle.open <= 0 || candle.close <= 0 || candle.high <= 0 ||
      candle.low <= 0) {
    Serial.printf(
        "ERROR: Negative/zero prices in candle O:%.2f H:%.2f L:%.2f C:%.2f\n",
        candle.open, candle.high, candle.low, candle.close);
    return false;
  }

  return true;
}

void DataFetcher::reset() {
  newest_candle_index = -1;
  num_candles = 0;
  last_update_time = 0;
  current_price = 0.0;
  initial_data_loaded = false;
}