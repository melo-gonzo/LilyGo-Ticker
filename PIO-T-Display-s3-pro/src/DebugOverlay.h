#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <lvgl.h>

void debug_init(lv_obj_t *parent);
void debug_print(const char *format, ...);
void debug_clear();

#endif // DEBUG_OVERLAY_H