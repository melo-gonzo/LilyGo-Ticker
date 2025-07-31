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
    buildIntradayCandle(price, now);

    // DEBUG: Print occasionally to verify timing
    static unsigned long lastTestDebug = 0;
    if (millis() - lastTestDebug > 5000) { // Every 5 seconds
      Serial.println(
          "Test data generated - Price: " + String(price) + ", Interval: " +
          String(INTRADAY_UPDATE_INTERVAL) + "ms" + ", Actual gap: " +
          String(millis() - (last_update_millis - INTRADAY_UPDATE_INTERVAL)) +
          "ms");
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
  int interval_seconds = getIntervalSeconds(YAHOO_INTERVAL);

  // Check if we should create a new candle or update the existing one
  if (num_candles == 0 ||
      shouldCreateNewCandle(timestamp, candles[newest_candle_index].timestamp,
                            interval_seconds)) {
    // Create new candle
    enhanced_candle_t newCandle;
    newCandle.timestamp = timestamp;
    newCandle.open = price;
    newCandle.high = price;
    newCandle.low = price;
    newCandle.close = price;
    newCandle.is_complete = true; // ALWAYS mark as complete - no orange bars

    updateCircularBuffer(newCandle);

    if (USE_TEST_DATA) {
      Serial.println("Test data: Created new candle at price " + String(price));
    }
  } else {
    // Update existing candle
    candles[newest_candle_index].close = price;
    candles[newest_candle_index].high =
        std::max(candles[newest_candle_index].high, price);
    candles[newest_candle_index].low =
        std::min(candles[newest_candle_index].low, price);
    candles[newest_candle_index].is_complete = true; // ALWAYS mark as complete

    if (USE_TEST_DATA) {
      Serial.println("Test data: Updated existing candle, close price: " +
                     String(price));
    }
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
  Serial.println("Initializing test data...");

  float base_price = 250.0;
  time_t now;
  time(&now);

  // IMPORTANT: Reset the buffer first to clear any existing real data
  reset();
  Serial.println("Cleared existing data for test mode");

  // Pre-populate with historical test data
  int test_candles =
      std::min(MAX_CANDLES - 5, 100); // Generate up to 100 test candles

  for (int i = 0; i < test_candles; i++) {
    enhanced_candle_t candle;

    // Generate realistic price movements
    float price_change = (random(-200, 201) / 100.0) * 2; // -4 to +4 change
    float open = base_price + price_change;

    // Generate intraday movement
    float intraday_range = (random(50, 200) / 100.0); // 0.5 to 2.0 range
    float close_change = (random(-100, 101) / 100.0) * intraday_range;
    float close = open + close_change;

    // Ensure prices don't go negative
    open = std::max(1.0f, open);
    close = std::max(1.0f, close);

    // Calculate high and low
    candle.high =
        std::max(open, close) + (random(0, 100) / 100.0) * intraday_range;
    candle.low =
        std::min(open, close) - (random(0, 100) / 100.0) * intraday_range;
    candle.open = open;
    candle.close = close;

    // Ensure low doesn't go negative
    candle.low = std::max(0.1f, candle.low);

    // Set timestamp going backwards in time
    candle.timestamp = now - (test_candles - i) * CANDLE_COLLECTION_DURATION;
    candle.is_complete = true; // FIXED: Mark all test data as complete

    updateCircularBuffer(candle);
    base_price = close; // Use close as next base price for continuity
  }

  current_price = candles[newest_candle_index].close;
  initial_data_loaded = true;

  Serial.println("Test data initialized with " + String(num_candles) +
                 " candles");
  Serial.println("Final price for continuation: " + String(current_price));

  // Print some sample data for verification
  for (int i = 0; i < std::min(3, num_candles); i++) {
    int idx = (newest_candle_index - i + MAX_CANDLES) % MAX_CANDLES;
    Serial.printf("Test candle %d: O:%.2f H:%.2f L:%.2f C:%.2f Complete:%d\n",
                  i, candles[idx].open, candles[idx].high, candles[idx].low,
                  candles[idx].close, candles[idx].is_complete);
  }
}

void DataFetcher::reset() {
  newest_candle_index = -1;
  num_candles = 0;
  last_update_time = 0;
  current_price = 0.0;
  initial_data_loaded = false;
}