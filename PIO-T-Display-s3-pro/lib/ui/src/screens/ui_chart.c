#include "ui.h"
#include "lvgl.h"

#define INFO_PANEL_WIDTH 80  // Same as in CandleStick.cpp

void ui_chart_screen_init(void)
{
    ui_chart = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_chart, LV_OBJ_FLAG_SCROLLABLE);

    // Get screen dimensions
    lv_coord_t screen_width = lv_disp_get_hor_res(lv_disp_get_default());
    lv_coord_t screen_height = lv_disp_get_ver_res(lv_disp_get_default());

    // Create a container for the chart
    lv_obj_t *chart_container = lv_obj_create(ui_chart);
    lv_obj_set_size(chart_container, screen_width, screen_height);
    lv_obj_align(chart_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(chart_container, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Remove padding to use full screen
    lv_obj_set_style_pad_all(chart_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Add a vertical divider line where the info panel will begin
    lv_obj_t *divider = lv_obj_create(chart_container);
    lv_obj_set_size(divider, 1, screen_height);
    lv_obj_set_style_bg_color(divider, lv_color_make(100, 100, 100), 0); // Light gray
    lv_obj_set_style_bg_opa(divider, LV_OPA_70, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_RIGHT, -INFO_PANEL_WIDTH, 0); // Position at INFO_PANEL_WIDTH from right

    // Store the chart container in a custom attribute of ui_chart for easy access
    lv_obj_set_user_data(ui_chart, chart_container);
}