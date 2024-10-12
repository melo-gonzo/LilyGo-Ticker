#include "TimeHelper.h"
#include "config.h"
#include "ui.h"
#include <Arduino.h>
#include <time.h>

static bool ntpSyncCompleted = false;

void initiateNTPTimeSync() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    // Wait for time to be set
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 10) {
        delay(500);
        now = time(nullptr);
        retries++;
    }
    
    if (now > 8 * 3600 * 2) {
        ntpSyncCompleted = true;
        Serial.println("NTP time sync completed");
    } else {
        Serial.println("NTP time sync failed");
    }
}

bool isTimeSynchronized() {
    return ntpSyncCompleted;
}

void updateTimeAndDate() {
    if (!ntpSyncCompleted) {
        return;
    }

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);

    // Update time label on the chart screen
    lv_obj_t *time_label = lv_obj_get_child(ui_chart, 0);
    if (time_label == NULL) {
        time_label = lv_label_create(ui_chart);
        lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    }
    lv_label_set_text_fmt(time_label, "%s\n%s", dateStr, timeStr);
}