#include "CandleStick.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include "ui.h"
#include "TimeHelper.h"
#include "config.h"

#define CANDLE_PADDING 0

static candle_t candles[MAX_CANDLES];
static long timestamps[MAX_CANDLES];
static int newest_candle_index = -1;
static int num_candles = 0;
static time_t last_update_time = 0;
static float current_price = 0.0;  // Added to store current price

#define API_URL "https://query1.finance.yahoo.com/v8/finance/chart/"


static float getRandomPrice() {
    static float lastPrice = 50.0; // Start at 50
    float change = (random(-100, 101) / 100.0) * 2; // Random change between -2 and 2
    lastPrice += change;
    lastPrice = max(0.0f, lastPrice); // Ensure price doesn't go negative
    current_price = lastPrice; // Update current price with random value
    return lastPrice;
}

static void update_circular_buffer(float currentPrice, time_t now) {
    if (num_candles < MAX_CANDLES) {
        num_candles++;
    }
    
    newest_candle_index = (newest_candle_index + 1) % MAX_CANDLES;

    candles[newest_candle_index].open = currentPrice;
    candles[newest_candle_index].high = currentPrice;
    candles[newest_candle_index].low = currentPrice;
    candles[newest_candle_index].close = currentPrice;
    timestamps[newest_candle_index] = now;
    current_price = currentPrice; // Update current price when updating buffer
}

static float getCurrentPrice(const char* symbol) {
    if (USE_TEST_DATA) {
        return getRandomPrice();
    }

    HTTPClient http;
    // https://query1.finance.yahoo.com/v8/finance/chart/BTC?interval=1d&range=1d
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + String(symbol) + "?interval=1d&range=1d";
    http.begin(url);
    int httpCode = http.GET();
    
    float price = 0.0;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            JsonObject chart = doc["chart"]["result"][0];
            JsonObject quote = chart["indicators"]["quote"][0];
            price = quote["close"][0];
        }
    }

    http.end();
    current_price = price;  // Store the current price
    return price;
}

bool fetch_intraday_data(const char *symbol) {
    time_t now;
    time(&now);
    
    if (now - last_update_time < INTRADAY_UPDATE_INTERVAL) {
        return false;
    }
    
    last_update_time = now;
    float currentPrice = getCurrentPrice(symbol);
    
    if (currentPrice == 0.0) {
        return false;
    }
    
    current_price = currentPrice; // Update current price
    
    if (num_candles == 0 || (now / CANDLE_COLLECTION_DURATION) != (timestamps[newest_candle_index] / CANDLE_COLLECTION_DURATION)) {
        update_circular_buffer(currentPrice, now);
    } else {
        candles[newest_candle_index].close = currentPrice;
        candles[newest_candle_index].high = max(candles[newest_candle_index].high, currentPrice);
        candles[newest_candle_index].low = min(candles[newest_candle_index].low, currentPrice);
    }
    
    return true;
}

bool fetch_candle_data(const char *symbol) {
    HTTPClient http;
    String url = String(API_URL) + String(symbol) + "?interval=1d&range=2y";
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        http.end();
        return false;
    }

    JsonArray timestamp = doc["chart"]["result"][0]["timestamp"].as<JsonArray>();
    JsonObject quote = doc["chart"]["result"][0]["indicators"]["quote"][0];
    JsonArray open = quote["open"].as<JsonArray>();
    JsonArray high = quote["high"].as<JsonArray>();
    JsonArray low = quote["low"].as<JsonArray>();
    JsonArray close = quote["close"].as<JsonArray>();

    int dataSize = min((int)timestamp.size(), MAX_CANDLES);

    // Reset the circular buffer
    num_candles = 0;
    newest_candle_index = -1;

    // Store the data in order, with the most recent data at the end
    for (int i = 0; i < dataSize; i++) {
        int dataIndex = timestamp.size() - dataSize + i;
        update_circular_buffer(open[dataIndex].as<float>(), timestamp[dataIndex].as<long>());
        candles[newest_candle_index].close = close[dataIndex].as<float>();
        candles[newest_candle_index].high = high[dataIndex].as<float>();
        candles[newest_candle_index].low = low[dataIndex].as<float>();
    }

    http.end();
    return true;
}

static void get_price_levels(float *min_price, float *max_price) {
    *min_price = std::numeric_limits<float>::max();
    *max_price = std::numeric_limits<float>::lowest();
    bool has_data = false;

    for (int i = 0; i < num_candles; i++) {
        int index = (newest_candle_index - num_candles + i + 1 + MAX_CANDLES) % MAX_CANDLES;
        *min_price = std::min(*min_price, candles[index].low);
        *max_price = std::max(*max_price, candles[index].high);
        has_data = true;
    }

    if (!has_data) {
        *min_price = 0;
        *max_price = 100;
    }

    // Round to 2 decimal places
    *min_price = floor(*min_price * 100) / 100;
    *max_price = ceil(*max_price * 100) / 100;
}

static void draw_candlestick(lv_obj_t *parent, int index, float open, float close, float high, float low, float min_price, float max_price) {
    lv_coord_t chart_width = lv_obj_get_width(parent);
    lv_coord_t chart_height = lv_obj_get_height(parent);

    int candle_width = (chart_width / MAX_CANDLES) - CANDLE_PADDING;
    int x = index * (candle_width + CANDLE_PADDING);

    int y_top = chart_height * (1.0f - (high - min_price) / (max_price - min_price));
    int y_bottom = chart_height * (1.0f - (low - min_price) / (max_price - min_price));
    int y_open = chart_height * (1.0f - (open - min_price) / (max_price - min_price));
    int y_close = chart_height * (1.0f - (close - min_price) / (max_price - min_price));

    y_top = constrain(y_top, 0, chart_height);
    y_bottom = constrain(y_bottom, 0, chart_height);
    y_open = constrain(y_open, 0, chart_height);
    y_close = constrain(y_close, 0, chart_height);

    lv_color_t candle_color = (close >= open) ? 
        lv_color_make(0, 255, 0) :  // Bright green 
        lv_color_make(255, 0, 0);   // Bright red

    // Draw wick
    lv_obj_t *wick = lv_obj_create(parent);
    lv_obj_set_size(wick, 1, y_bottom - y_top);
    lv_obj_set_style_bg_color(wick, candle_color, 0);
    lv_obj_set_style_bg_opa(wick, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wick, 0, 0);  // Remove border
    lv_obj_align(wick, LV_ALIGN_TOP_LEFT, x + (candle_width / 2), y_top);

    // Draw body
    lv_obj_t *body = lv_obj_create(parent);
    int body_y = (y_open < y_close) ? y_open : y_close;
    lv_obj_set_size(body, candle_width, abs(y_close - y_open));
    lv_obj_set_style_bg_color(body, candle_color, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(body, 0, 0);  // Remove border
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, x, body_y);
}

static void draw_current_price_line(lv_obj_t *parent, float current_price, float min_price, float max_price) {
    lv_coord_t chart_width = lv_obj_get_width(parent);
    lv_coord_t chart_height = lv_obj_get_height(parent);

    // Calculate y position for current price
    int y_current = chart_height * (1.0f - (current_price - min_price) / (max_price - min_price));
    y_current = constrain(y_current, 0, chart_height);

    // Draw horizontal line
    lv_obj_t *price_line = lv_obj_create(parent);
    lv_obj_set_size(price_line, chart_width, 2);  // Made line slightly thicker
    lv_obj_set_style_bg_color(price_line, lv_color_make(0, 255, 255), 0);  // Cyan color for better visibility
    lv_obj_set_style_bg_opa(price_line, LV_OPA_70, 0);  // More opaque
    lv_obj_set_style_border_width(price_line, 0, 0);
    lv_obj_align(price_line, LV_ALIGN_TOP_LEFT, 0, y_current);

    // Create current price label on the right
    lv_obj_t *current_label = lv_label_create(parent);
    char price_str[32];
    snprintf(price_str, sizeof(price_str), "%.2f", current_price);
    lv_label_set_text(current_label, price_str);
    
    // Style the current price label
    lv_obj_set_style_text_color(current_label, lv_color_make(0, 255, 255), 0);  // Cyan text
    lv_obj_set_style_bg_color(current_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(current_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(current_label, 5, 0);  // Add more padding
    lv_obj_set_style_radius(current_label, 4, 0);   // Rounded corners
    
    // Position the label on the right side at the current price level
    lv_obj_align(current_label, LV_ALIGN_TOP_RIGHT, -5, y_current - 10);
}

void candle_stick_create(lv_obj_t *parent, const char *symbol) {
    lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(parent);
    if (chart_container == NULL) {
        return;
    }
    lv_obj_clean(chart_container);

    lv_obj_set_style_bg_color(chart_container, lv_color_black(), 0);

    float min_price, max_price;
    get_price_levels(&min_price, &max_price);

    // Add padding for drawing
    float range = max_price - min_price;
    float padding = range * 0.1; // 10% padding
    float draw_min = std::max(min_price - padding, 0.0f);
    float draw_max = max_price + padding;

    // Draw candlesticks
    for (int i = 0; i < num_candles; i++) {
        int index = (newest_candle_index - num_candles + i + 1 + MAX_CANDLES) % MAX_CANDLES;
        draw_candlestick(chart_container, i, candles[index].open, 
                         candles[index].close, 
                         candles[index].high, 
                         candles[index].low, 
                         draw_min, draw_max);
    }

    // Always draw current price line since we're maintaining the current_price
    draw_current_price_line(chart_container, current_price, draw_min, draw_max);

    // Add price labels
    char min_price_str[16], max_price_str[16];
    snprintf(min_price_str, sizeof(min_price_str), "%.2f", min_price);
    snprintf(max_price_str, sizeof(max_price_str), "%.2f", max_price);

    lv_obj_t *min_label = lv_label_create(chart_container);
    lv_obj_t *max_label = lv_label_create(chart_container);

    lv_label_set_text(min_label, min_price_str);
    lv_label_set_text(max_label, max_price_str);

    lv_obj_align(min_label, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    lv_obj_align(max_label, LV_ALIGN_TOP_LEFT, 5, 5);

    lv_obj_set_style_text_color(min_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(max_label, lv_color_white(), 0);

    lv_task_handler();
}

void update_intraday_data(const char *symbol) {
    if (USE_INTRADAY_DATA) {
        fetch_intraday_data(symbol);
        candle_stick_create(ui_chart, symbol);
    }
}

void set_intraday_parameters(int update_interval, int collection_duration) {
    INTRADAY_UPDATE_INTERVAL = update_interval;
    CANDLE_COLLECTION_DURATION = collection_duration;
}