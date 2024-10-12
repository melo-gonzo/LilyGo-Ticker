#include "ui.h"
#include "ui_helpers.h"
#include "lvgl.h"

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

    // Store the chart container in a custom attribute of ui_chart for easy access
    lv_obj_set_user_data(ui_chart, chart_container);
}