// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.11
// Project name: BTC-Ticker
#include <Arduino.h>
#include "ui.h"


void load_Screen_chart(lv_event_t * e)
{

    // Fetch the candlestick data and create the chart
    if (fetch_candle_data()) {
        candle_stick_create(ui_chart);
    }

}
