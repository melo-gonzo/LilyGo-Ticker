#ifndef ENHANCED_CANDLE_STICK_H
#define ENHANCED_CANDLE_STICK_H

#include <lvgl.h>
#include "data_fetcher.h"

#define CANDLE_PADDING 0
#define INFO_PANEL_WIDTH 80

// Define IDs for our user data objects
#define INFO_PANEL_ID 0x1001
#define SYMBOL_LABEL_ID 0x1002
#define PRICE_LABEL_ID 0x1003
#define HIGH_LABEL_ID 0x1004
#define LOW_LABEL_ID 0x1005
#define TIME_LABEL_ID 0x1006
#define DATE_LABEL_ID 0x1007
#define STATUS_LABEL_ID 0x1008
#define INTERVAL_LABEL_ID 0x1009

class EnhancedCandleStick {
private:
    static lv_obj_t* find_obj_by_id(lv_obj_t *parent, uint32_t id);
    static void draw_candlestick(lv_obj_t *parent, int index, const enhanced_candle_t& candle, 
                               float min_price, float max_price);
    static void draw_current_price_line(lv_obj_t *parent, float current_price, 
                                      float min_price, float max_price);
    static void draw_price_gridlines(lv_obj_t *parent, float min_price, float max_price);
    static void create_info_panel(lv_obj_t *parent, const String& symbol, 
                                float current_price, float min_price, float max_price);
    static void update_info_panel(lv_obj_t *chart_container, const String& symbol);

public:
    static void create(lv_obj_t *parent, const String& symbol);
    static void update(lv_obj_t *parent, const String& symbol);
};

#endif // ENHANCED_CANDLE_STICK_H