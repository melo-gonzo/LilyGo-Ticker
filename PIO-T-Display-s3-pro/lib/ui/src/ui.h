#ifndef _STOCK_TICKER_UI_H
#define _STOCK_TICKER_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_chart;

void ui_init(void);
void ui_chart_screen_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif