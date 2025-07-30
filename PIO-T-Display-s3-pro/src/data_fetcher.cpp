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

bool DataFetcher::initialize(const String& symbol) {
    reset();
    current_symbol = symbol;
    current_interval = YAHOO_INTERVAL;
    current_range = YAHOO_RANGE;
    
    if (USE_TEST_DATA) {
        initializeTestData();
        return true;
    }
    
    // Fetch initial historical data
    return fetchInitialData(symbol, YAHOO_INTERVAL, YAHOO_RANGE);
}

bool DataFetcher::fetchInitialData(const String& symbol, const String& interval, const String& range) {
    Serial.println("Fetching initial data for " + symbol + " with interval=" + interval + " range=" + range);
    
    HTTPClient http;
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + symbol + 
                 "?interval=" + interval + "&range=" + range;
    
    Serial.println("URL: " + url);
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.println("HTTP request failed with code: " + String(httpCode));
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        return false;
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
    int startIndex = timestamps.size() - dataSize; // Start from the most recent data that fits
    
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
    
    Serial.println("Loaded " + String(num_candles) + " candles. Current price: " + String(current_price));
    return true;
}

bool DataFetcher::updateData() {
    time_t now;
    time(&now);
    
    // Check if we need to update
    if (now - last_update_time < INTRADAY_UPDATE_INTERVAL) {
        return false;
    }
    
    last_update_time = now;
    
    if (USE_TEST_DATA) {
        float price = getRandomPrice();
        buildIntradayCandle(price, now);
        return true;
    }
    
    // Check market hours if enforced
    if (ENFORCE_MARKET_HOURS && !StockTracker::MarketHoursChecker::isMarketOpen()) {
        return false;
    }
    
    // Fetch current price for intraday candle building
    HTTPClient http;
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + current_symbol + 
                 "?interval=1m&range=1d";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        return false;
    }
    
    JsonArray timestamps = doc["chart"]["result"][0]["timestamp"].as<JsonArray>();
    JsonObject quote = doc["chart"]["result"][0]["indicators"]["quote"][0];
    JsonArray closes = quote["close"].as<JsonArray>();
    
    if (timestamps.size() == 0 || closes.size() == 0) {
        return false;
    }
    
    // Get the most recent price
    float latestPrice = 0.0;
    for (int i = closes.size() - 1; i >= 0; i--) {
        if (!closes[i].isNull()) {
            latestPrice = closes[i].as<float>();
            break;
        }
    }
    
    if (latestPrice > 0) {
        current_price = latestPrice;
        buildIntradayCandle(latestPrice, now);
        return true;
    }
    
    return false;
}

void DataFetcher::buildIntradayCandle(float price, time_t timestamp) {
    int interval_seconds = getIntervalSeconds(YAHOO_INTERVAL);
    
    // Check if we should create a new candle or update the existing one
    if (num_candles == 0 || shouldCreateNewCandle(timestamp, candles[newest_candle_index].timestamp, interval_seconds)) {
        // Create new candle
        enhanced_candle_t newCandle;
        newCandle.timestamp = timestamp;
        newCandle.open = price;
        newCandle.high = price;
        newCandle.low = price;
        newCandle.close = price;
        newCandle.is_complete = false;
        
        updateCircularBuffer(newCandle);
    } else {
        // Update existing candle
        candles[newest_candle_index].close = price;
        candles[newest_candle_index].high = std::max(candles[newest_candle_index].high, price);
        candles[newest_candle_index].low = std::min(candles[newest_candle_index].low, price);
        
        // Check if candle should be marked as complete
        time_t candle_end_time = candles[newest_candle_index].timestamp + interval_seconds;
        if (timestamp >= candle_end_time) {
            candles[newest_candle_index].is_complete = true;
        }
    }
    
    current_price = price;
}

bool DataFetcher::shouldCreateNewCandle(time_t current_time, time_t last_candle_time, int interval_seconds) {
    return (current_time / interval_seconds) != (last_candle_time / interval_seconds);
}

int DataFetcher::getIntervalSeconds(const String& interval) {
    if (interval == "1m") return 60;
    if (interval == "2m") return 120;
    if (interval == "5m") return 300;
    if (interval == "15m") return 900;
    if (interval == "30m") return 1800;
    if (interval == "60m" || interval == "1h") return 3600;
    if (interval == "90m") return 5400;
    if (interval == "1d") return 86400;
    
    // Default to the configured candle collection duration
    return CANDLE_COLLECTION_DURATION;
}

void DataFetcher::updateCircularBuffer(const enhanced_candle_t& candle) {
    if (num_candles < MAX_CANDLES) {
        num_candles++;
    }
    
    newest_candle_index = (newest_candle_index + 1) % MAX_CANDLES;
    candles[newest_candle_index] = candle;
}

void DataFetcher::getPriceLevels(float* min_price, float* max_price) {
    *min_price = std::numeric_limits<float>::max();
    *max_price = std::numeric_limits<float>::lowest();
    
    if (num_candles == 0) {
        *min_price = 0;
        *max_price = 100;
        return;
    }
    
    for (int i = 0; i < num_candles; i++) {
        int index = (newest_candle_index - num_candles + i + 1 + MAX_CANDLES) % MAX_CANDLES;
        *min_price = std::min(*min_price, candles[index].low);
        *max_price = std::max(*max_price, candles[index].high);
    }
    
    // Round to 2 decimal places
    *min_price = floor(*min_price * 100) / 100;
    *max_price = ceil(*max_price * 100) / 100;
}

float DataFetcher::getRandomPrice() {
    static float lastPrice = 250.0;
    float change = (random(-100, 101) / 100.0) * 2;
    lastPrice += change;
    lastPrice = std::max(0.0f, lastPrice);
    return lastPrice;
}

void DataFetcher::initializeTestData() {
    float base_price = 250.0;
    time_t now;
    time(&now);
    
    reset();
    
    // Pre-populate with historical data
    for (int i = 0; i < MAX_CANDLES - 5; i++) {
        enhanced_candle_t candle;
        
        float open = base_price + (random(-100, 101) / 100.0) * 2;
        float close = open + (random(-100, 101) / 100.0) * 1.5;
        candle.high = std::max(open, close) + (random(0, 51) / 100.0);
        candle.low = std::min(open, close) - (random(0, 51) / 100.0);
        candle.open = open;
        candle.close = close;
        candle.timestamp = now - (MAX_CANDLES - 5 - i) * CANDLE_COLLECTION_DURATION;
        candle.is_complete = true;
        
        base_price = close;
        updateCircularBuffer(candle);
    }
    
    current_price = candles[newest_candle_index].close;
    initial_data_loaded = true;
    
    Serial.println("Test data initialized with " + String(num_candles) + " candles");
}

void DataFetcher::reset() {
    newest_candle_index = -1;
    num_candles = 0;
    last_update_time = 0;
    current_price = 0.0;
    initial_data_loaded = false;
}