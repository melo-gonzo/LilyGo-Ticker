#include "time_helper.h"
#include "config.h"
#include "ui.h"
#include <Arduino.h>
#include <time.h>

static bool ntpSyncCompleted = false;

#define INFO_PANEL_ID 0x1001
#define SYMBOL_LABEL_ID 0x1002
#define PRICE_LABEL_ID 0x1003
#define HIGH_LABEL_ID 0x1004
#define LOW_LABEL_ID 0x1005
#define TIME_LABEL_ID 0x1006
#define DATE_LABEL_ID 0x1007

void initiateNTPTimeSync() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", TIME_ZONE, 1);
    tzset();
    
    // Wait for time to be set
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 20) { // Increased retries for more reliability
        delay(500);
        Serial.println("Waiting for NTP time sync...");
        now = time(nullptr);
        retries++;
    }
    
    if (now > 8 * 3600 * 2) {
        ntpSyncCompleted = true;
        Serial.println("NTP time sync completed successfully");
        
        // Print current time to verify
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
        Serial.printf("Current time: %s\n", timeStr);
    } else {
        Serial.println("NTP time sync failed after retries");
    }
}

bool isTimeSynchronized() {
    return ntpSyncCompleted;
}

// Find an object with specific ID in a container - same as in CandleStick.cpp
static lv_obj_t* find_obj_by_id(lv_obj_t *parent, uint32_t id) {
    if (parent == NULL) {
        return NULL;
    }
    
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

void updateTimeAndDate() {
    if (!ntpSyncCompleted) {
        // Try to sync if not already synchronized
        static unsigned long lastSyncAttempt = 0;
        if (millis() - lastSyncAttempt > 60000) { // Try every minute
            initiateNTPTimeSync();
            lastSyncAttempt = millis();
        }
        return;
    }

    // Only update time strings every second
    static unsigned long lastTimeStringUpdate = 0;
    static char timeStr[9] = "00:00:00";
    static char dateStr[9] = "00-00-00";  // Changed to MM-DD-YY format
    
    // Update the time strings once per second
    if (millis() - lastTimeStringUpdate > 1000) {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        strftime(dateStr, sizeof(dateStr), "%m-%d-%y", &timeinfo);  // MM-DD-YY format
        
        lastTimeStringUpdate = millis();
        
        // Debug output for date format verification (remove after testing)
        static unsigned long lastDebugOutput = 0;
        if (millis() - lastDebugOutput > 30000) { // Debug every 30 seconds
            Serial.printf("Date display format: %s (MM-DD-YY)\n", dateStr);
            lastDebugOutput = millis();
        }
    }
    
    // Check if we have an info panel on the chart screen
    lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(ui_chart);
    if (chart_container != NULL) {
        // Find the info panel by its ID
        lv_obj_t *info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
        
        if (info_panel != NULL) {
            // Find time and date labels by their IDs
            lv_obj_t *time_label = find_obj_by_id(info_panel, TIME_LABEL_ID);
            lv_obj_t *date_label = find_obj_by_id(info_panel, DATE_LABEL_ID);
            
            // Create time label if it doesn't exist
            if (time_label == NULL) {
                time_label = lv_label_create(info_panel);
                lv_obj_set_user_data(time_label, (void*)TIME_LABEL_ID);
                lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
                lv_obj_align(time_label, LV_ALIGN_BOTTOM_MID, 0, -30);
            }
            
            // Create date label if it doesn't exist
            if (date_label == NULL) {
                date_label = lv_label_create(info_panel);
                lv_obj_set_user_data(date_label, (void*)DATE_LABEL_ID);
                lv_obj_set_style_text_color(date_label, lv_color_white(), 0);
                lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -10);
            }
            
            // Check if text has changed before updating to avoid flicker
            const char* current_time_text = lv_label_get_text(time_label);
            const char* current_date_text = lv_label_get_text(date_label);
            
            if (strcmp(current_time_text, timeStr) != 0) {
                lv_label_set_text(time_label, timeStr);
            }
            
            if (strcmp(current_date_text, dateStr) != 0) {
                lv_label_set_text(date_label, dateStr);
            }
        }
    }
}