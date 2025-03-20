#include "CandleStick.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include "ui.h"
#include "TimeHelper.h"
#include "config.h"
#include "market_hours.h"

#define CANDLE_PADDING 0
#define INFO_PANEL_WIDTH 80  // Width in pixels for the right side info panel
// Define IDs for our user data objects
#define INFO_PANEL_ID 0x1001
#define SYMBOL_LABEL_ID 0x1002
#define PRICE_LABEL_ID 0x1003
#define HIGH_LABEL_ID 0x1004
#define LOW_LABEL_ID 0x1005
#define TIME_LABEL_ID 0x1006
#define DATE_LABEL_ID 0x1007


static candle_t candles[MAX_CANDLES];
static long timestamps[MAX_CANDLES];
static int newest_candle_index = -1;
static int num_candles = 0;
static time_t last_update_time = 0;
static float current_price = 0.0;  // Added to store current price

#define API_URL "https://query1.finance.yahoo.com/v8/finance/chart/"


static float getRandomPrice() {
    static float lastPrice = 250.0; // Start at 250
    float change = (random(-100, 101) / 100.0) * 2; // Random change between -2 and 2
    lastPrice += change;
    lastPrice = max(0.0f, lastPrice); // Ensure price doesn't go negative
    current_price = lastPrice; // Update current price with random value
    return lastPrice;
}

void initialize_test_data() {
    if (!USE_TEST_DATA) {
        return;  // Only use for test data mode
    }
    
    // Start with a reasonable base price
    float base_price = 250.0;
    
    // Get the current time
    time_t now;
    time(&now);
    
    // Reset the candle buffer
    num_candles = 0;
    newest_candle_index = -1;
    
    // Pre-populate with historical data
    // We'll create MAX_CANDLES - 5 historical candles, leaving room for 5 new ones
    for (int i = 0; i < MAX_CANDLES - 5; i++) {
        // Create some random price movement
        float open = base_price + (random(-100, 101) / 100.0) * 2;
        float close = open + (random(-100, 101) / 100.0) * 1.5;
        float high = max(open, close) + (random(0, 51) / 100.0);
        float low = min(open, close) - (random(0, 51) / 100.0);
        
        // Move the base price to create a trend
        base_price = close;
        
        // Calculate timestamp for this candle
        // Each candle is one collection duration in the past
        time_t candle_time = now - (MAX_CANDLES - 5 - i) * CANDLE_COLLECTION_DURATION;
        
        // Add to the circular buffer
        newest_candle_index = (newest_candle_index + 1) % MAX_CANDLES;
        num_candles++;
        
        candles[newest_candle_index].open = open;
        candles[newest_candle_index].close = close;
        candles[newest_candle_index].high = high;
        candles[newest_candle_index].low = low;
        timestamps[newest_candle_index] = candle_time;
    }
    
    // Set current price to the last close price
    current_price = candles[newest_candle_index].close;
    
    Serial.println("Test data initialized with " + String(num_candles) + " candles");
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
    // Calculate chart width excluding the info panel
    lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
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
    // Calculate chart width excluding the info panel
    lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
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
}

static void draw_price_gridlines(lv_obj_t *parent, float min_price, float max_price) {
    // Calculate chart width excluding the info panel
    lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
    lv_coord_t chart_height = lv_obj_get_height(parent);
    
    // Find the first whole dollar above min_price
    float first_line = ceil(min_price);
    // Find the last whole dollar below max_price
    float last_line = floor(max_price);
    
    // Light gray color for grid lines
    lv_color_t grid_color = lv_color_make(100, 100, 100); // Light gray
    
    // Draw a horizontal line for each whole dollar price
    for (float price = first_line; price <= last_line; price += 1.0f) {
        // Calculate y position
        int y_pos = chart_height * (1.0f - (price - min_price) / (max_price - min_price));
        y_pos = constrain(y_pos, 0, chart_height);
        
        // Create horizontal line
        lv_obj_t *grid_line = lv_obj_create(parent);
        lv_obj_set_size(grid_line, chart_width, 1); // 1px thin line
        lv_obj_set_style_bg_color(grid_line, grid_color, 0);
        lv_obj_set_style_bg_opa(grid_line, LV_OPA_50, 0); // Semi-transparent
        lv_obj_set_style_border_width(grid_line, 0, 0);
        lv_obj_align(grid_line, LV_ALIGN_TOP_LEFT, 0, y_pos);
    }
}

static lv_obj_t* find_obj_by_id(lv_obj_t *parent, uint32_t id) {
    uint32_t child_cnt = lv_obj_get_child_cnt(parent);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (child != NULL) {
            uint32_t child_id = (uint32_t)lv_obj_get_user_data(child);
            if (child_id == id) {
                return child;
            }
        }
    }
    return NULL;
}


// Create info panel function - this should be a standalone function
static void create_info_panel(lv_obj_t *parent, const char *symbol, float current_price, float min_price, float max_price) {
    // Calculate dimensions
    lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
    lv_coord_t chart_height = lv_obj_get_height(parent);
    
    // Find if the info panel already exists
    lv_obj_t *info_panel = find_obj_by_id(parent, INFO_PANEL_ID);
    
    // Create the panel if it doesn't exist
    if (info_panel == NULL) {
        info_panel = lv_obj_create(parent);
        lv_obj_set_size(info_panel, INFO_PANEL_WIDTH, chart_height);
        lv_obj_align(info_panel, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(info_panel, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(info_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(info_panel, 0, 0);
        lv_obj_set_user_data(info_panel, (void*)INFO_PANEL_ID);  // Set ID for identification
        
        // Create labels inside the panel
        lv_obj_t *symbol_label = lv_label_create(info_panel);
        lv_label_set_text(symbol_label, symbol);
        lv_obj_align(symbol_label, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_text_color(symbol_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(symbol_label, &lv_font_montserrat_16, 0);
        lv_obj_set_user_data(symbol_label, (void*)SYMBOL_LABEL_ID);
        
        // Current price label
        lv_obj_t *price_label = lv_label_create(info_panel);
        char price_buf[16];
        snprintf(price_buf, sizeof(price_buf), "%.2f", current_price);
        lv_label_set_text(price_label, price_buf);
        lv_obj_align(price_label, LV_ALIGN_TOP_MID, 0, 40);
        lv_obj_set_style_text_color(price_label, lv_color_make(0, 255, 255), 0);
        // Use a font that exists in your version
        lv_obj_set_style_text_font(price_label, &lv_font_montserrat_16, 0);
        lv_obj_set_user_data(price_label, (void*)PRICE_LABEL_ID);
        
        // Add high/low labels
        lv_obj_t *high_label = lv_label_create(info_panel);
        lv_obj_t *low_label = lv_label_create(info_panel);
        
        char high_buf[32], low_buf[32];
        snprintf(high_buf, sizeof(high_buf), "H: %.2f", max_price);
        snprintf(low_buf, sizeof(low_buf), "L: %.2f", min_price);
        
        lv_label_set_text(high_label, high_buf);
        lv_label_set_text(low_label, low_buf);
        
        lv_obj_align(high_label, LV_ALIGN_TOP_MID, 0, 75);
        lv_obj_align(low_label, LV_ALIGN_TOP_MID, 0, 95);
        
        lv_obj_set_style_text_color(high_label, lv_color_make(0, 255, 0), 0);
        lv_obj_set_style_text_color(low_label, lv_color_make(255, 0, 0), 0);
        
        lv_obj_set_user_data(high_label, (void*)HIGH_LABEL_ID);
        lv_obj_set_user_data(low_label, (void*)LOW_LABEL_ID);
    } else {
        // Update existing labels
        lv_obj_t *symbol_label = find_obj_by_id(info_panel, SYMBOL_LABEL_ID);
        if (symbol_label != NULL) {
            lv_label_set_text(symbol_label, symbol);
        }
        
        lv_obj_t *price_label = find_obj_by_id(info_panel, PRICE_LABEL_ID);
        if (price_label != NULL) {
            char price_buf[16];
            snprintf(price_buf, sizeof(price_buf), "%.2f", current_price);
            lv_label_set_text(price_label, price_buf);
        }
        
        lv_obj_t *high_label = find_obj_by_id(info_panel, HIGH_LABEL_ID);
        if (high_label != NULL) {
            char high_buf[32];
            snprintf(high_buf, sizeof(high_buf), "H: %.2f", max_price);
            lv_label_set_text(high_label, high_buf);
        }
        
        lv_obj_t *low_label = find_obj_by_id(info_panel, LOW_LABEL_ID);
        if (low_label != NULL) {
            char low_buf[32];
            snprintf(low_buf, sizeof(low_buf), "L: %.2f", min_price);
            lv_label_set_text(low_label, low_buf);
        }
    }
    // Calculate y position for current price (for visual reference)
    int y_current = chart_height * (1.0f - (current_price - min_price) / (max_price - min_price));
    y_current = constrain(y_current, 0, chart_height);
    
}



void candle_stick_create(lv_obj_t *parent, const char *symbol) {
    lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(parent);
    if (chart_container == NULL) {
        return;
    }
    
    // Save time and date labels text before cleaning
    lv_obj_t *saved_info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
    char time_text[16] = "";
    char date_text[16] = "";
    
    if (saved_info_panel != NULL) {
        lv_obj_t *time_label = find_obj_by_id(saved_info_panel, TIME_LABEL_ID);
        lv_obj_t *date_label = find_obj_by_id(saved_info_panel, DATE_LABEL_ID);
        
        // Save the text content instead of the object pointers
        if (time_label != NULL) {
            strncpy(time_text, lv_label_get_text(time_label), sizeof(time_text) - 1);
        }
        
        if (date_label != NULL) {
            strncpy(date_text, lv_label_get_text(date_label), sizeof(date_text) - 1);
        }
    }
    
    // Get any existing info panel before cleaning
    char symbol_text[32] = "";
    float saved_price = 0.0;
    float saved_high = 0.0;
    float saved_low = 0.0;
    
    if (saved_info_panel != NULL) {
        lv_obj_t *symbol_label = find_obj_by_id(saved_info_panel, SYMBOL_LABEL_ID);
        if (symbol_label != NULL) {
            strncpy(symbol_text, lv_label_get_text(symbol_label), sizeof(symbol_text) - 1);
        }
    }
    
    // Now clean the entire container
    lv_obj_clean(chart_container);
    lv_obj_set_style_bg_color(chart_container, lv_color_black(), 0);

    float min_price, max_price;
    get_price_levels(&min_price, &max_price);

    // Add padding for drawing
    float range = max_price - min_price;
    float padding = range * 0.1; // 10% padding
    float draw_min = std::max(min_price - padding, 0.0f);
    float draw_max = max_price + padding;

    draw_price_gridlines(chart_container, draw_min, draw_max);

    // Draw candlesticks
    for (int i = 0; i < num_candles; i++) {
        int index = (newest_candle_index - num_candles + i + 1 + MAX_CANDLES) % MAX_CANDLES;
        draw_candlestick(chart_container, i, candles[index].open, 
                         candles[index].close, 
                         candles[index].high, 
                         candles[index].low, 
                         draw_min, draw_max);
    }

    // Draw current price line
    draw_current_price_line(chart_container, current_price, draw_min, draw_max);
    
    // Create or update the info panel
    create_info_panel(chart_container, symbol, current_price, min_price, max_price);
    
    // Recreate time and date labels with saved text
    lv_obj_t *new_info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
    if (new_info_panel != NULL) {
        // Recreate time label with saved text if it existed
        if (time_text[0] != '\0') {
            lv_obj_t *new_time_label = lv_label_create(new_info_panel);
            lv_label_set_text(new_time_label, time_text);
            lv_obj_align(new_time_label, LV_ALIGN_BOTTOM_MID, 0, -30);
            lv_obj_set_style_text_color(new_time_label, lv_color_white(), 0);
            lv_obj_set_user_data(new_time_label, (void*)TIME_LABEL_ID);
        }
        
        // Recreate date label with saved text if it existed
        if (date_text[0] != '\0') {
            lv_obj_t *new_date_label = lv_label_create(new_info_panel);
            lv_label_set_text(new_date_label, date_text);
            lv_obj_align(new_date_label, LV_ALIGN_BOTTOM_MID, 0, -10);
            lv_obj_set_style_text_color(new_date_label, lv_color_white(), 0);
            lv_obj_set_user_data(new_date_label, (void*)DATE_LABEL_ID);
        }
    }

    // Add price labels on the left
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
    static bool market_closed_border = false;
    
    if (USE_INTRADAY_DATA) {
        // Check market hours for real data
        bool is_market_open = USE_TEST_DATA || !ENFORCE_MARKET_HOURS || 
                             StockTracker::MarketHoursChecker::isMarketOpen();
        
        // Only fetch new data when market is open
        if (is_market_open) {
            // Fetch data only when market is open
            fetch_intraday_data(symbol);
        }
        
        // Create chart with existing data
        candle_stick_create(ui_chart, symbol);
        
        // Get the chart container
        lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(ui_chart);
        if (chart_container != NULL) {
            if (!is_market_open && !market_closed_border) {
                // Add orange border around the chart when market is closed
                lv_obj_set_style_border_width(chart_container, 3, 0);
                lv_obj_set_style_border_color(chart_container, lv_color_make(255, 140, 0), 0); // Orange
                market_closed_border = true;
                
                // Use the info panel to indicate market is closed
                lv_obj_t *info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
                if (info_panel != NULL) {
                    // Find status label or create if it doesn't exist
                    lv_obj_t *status_label = find_obj_by_id(info_panel, 0x1008); // New ID for status
                    if (status_label == NULL) {
                        status_label = lv_label_create(info_panel);
                        lv_obj_set_user_data(status_label, (void*)0x1008);
                        lv_obj_set_style_text_color(status_label, lv_color_make(255, 140, 0), 0); // Orange
                        lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 120); // Below other info
                    }
                    lv_label_set_text(status_label, "MARKET CLOSED");
                }
            } 
            else if (is_market_open && market_closed_border) {
                // Remove orange border when market opens
                lv_obj_set_style_border_width(chart_container, 0, 0);
                market_closed_border = false;
                
                // Remove closed market status label if it exists
                lv_obj_t *info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
                if (info_panel != NULL) {
                    lv_obj_t *status_label = find_obj_by_id(info_panel, 0x1008);
                    if (status_label != NULL) {
                        lv_obj_del(status_label);
                    }
                }
            }
        }
    }
}

void set_intraday_parameters(int update_interval, int collection_duration) {
    INTRADAY_UPDATE_INTERVAL = update_interval;
    CANDLE_COLLECTION_DURATION = collection_duration;
}