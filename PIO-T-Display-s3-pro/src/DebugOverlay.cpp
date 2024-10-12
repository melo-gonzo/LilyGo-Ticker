#include "DebugOverlay.h"
#include <stdio.h>
#include <stdarg.h>

#define MAX_DEBUG_LINES 10
#define MAX_LINE_LENGTH 50

static lv_obj_t *debug_label;
static char debug_buffer[MAX_DEBUG_LINES][MAX_LINE_LENGTH];
static int current_line = 0;

void debug_init(lv_obj_t *parent) {
    debug_label = lv_label_create(parent);
    lv_obj_set_size(debug_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(debug_label, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_text_color(debug_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(debug_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(debug_label, LV_OPA_50, 0);
    lv_label_set_long_mode(debug_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_width(debug_label, LV_HOR_RES - 10);
    debug_clear();
}

void debug_print(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    vsnprintf(debug_buffer[current_line], MAX_LINE_LENGTH, format, args);
    
    va_end(args);

    current_line = (current_line + 1) % MAX_DEBUG_LINES;

    char full_buffer[MAX_DEBUG_LINES * MAX_LINE_LENGTH] = {0};
    for (int i = 0; i < MAX_DEBUG_LINES; i++) {
        int index = (current_line + i) % MAX_DEBUG_LINES;
        if (debug_buffer[index][0] != '\0') {
            strcat(full_buffer, debug_buffer[index]);
            strcat(full_buffer, "\n");
        }
    }

    lv_label_set_text(debug_label, full_buffer);
    lv_task_handler();
}

void debug_clear() {
    memset(debug_buffer, 0, sizeof(debug_buffer));
    current_line = 0;
    lv_label_set_text(debug_label, "");
    lv_task_handler();
}