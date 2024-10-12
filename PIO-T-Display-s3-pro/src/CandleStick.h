#ifndef CANDLESTICK_H
#define CANDLESTICK_H

#include <lvgl.h>
#include <limits>
#include <time.h>
#include "config.h"

typedef struct {
    float open;
    float close;
    float high;
    float low;
} candle_t;

bool fetch_candle_data(const char *symbol);
bool fetch_intraday_data(const char *symbol);
void candle_stick_create(lv_obj_t *parent, const char *symbol);
void update_intraday_data(const char *symbol);
void set_intraday_parameters(int update_interval, int collection_duration);

#endif // CANDLESTICK_H