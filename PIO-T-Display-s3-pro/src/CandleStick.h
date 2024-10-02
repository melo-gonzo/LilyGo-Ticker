#ifndef CANDLESTICK_H
#define CANDLESTICK_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function to fetch and display the candlestick chart from API
void candle_stick_create(lv_obj_t *parent);

// Declare the fetch_candle_data function here
bool fetch_candle_data();

#ifdef __cplusplus
}
#endif

#endif // CANDLESTICK_H
