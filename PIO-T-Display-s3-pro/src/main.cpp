#include <Arduino.h>
#include <LV_Helper.h>
#include <LilyGo_AMOLED.h>
#include <lvgl.h>
#include "ui.h"
#include "CandleStick.h"
#include "TimeHelper.h"
#include "WiFiProvHelper.h"
#include "DebugOverlay.h"
#include "config.h"
#include "market_hours.h"


LilyGo_Class amoled;
const int resetPin = 0;
const unsigned long resetTime = 5000;

unsigned long resetButtonPressedTime = 0;
bool resetButtonState = false;

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!amoled.begin()) {
        while (1) delay(1000);
    }

    amoled.setRotation(0);
    amoled.setBrightness(125);

    beginLvglHelper(amoled);
    ui_init();

    lv_scr_act();

    if (USE_TEST_DATA) {
        set_intraday_parameters(INTRADAY_UPDATE_INTERVAL, CANDLE_COLLECTION_DURATION);
    } else {
        set_intraday_parameters(INTRADAY_UPDATE_INTERVAL, CANDLE_COLLECTION_DURATION);
    }
    if (USE_TEST_DATA) {
        set_intraday_parameters(INTRADAY_UPDATE_INTERVAL, CANDLE_COLLECTION_DURATION);
        initialize_test_data();  // Initialize with historical test data
    } else {
        set_intraday_parameters(INTRADAY_UPDATE_INTERVAL, CANDLE_COLLECTION_DURATION);
    }

    if (isProvisioned()) {
        WiFi.begin();

        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
            lv_task_handler();
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            initiateNTPTimeSync();

            if (USE_INTRADAY_DATA) {
                fetch_intraday_data(STOCK_SYMBOL);
            } else {
                fetch_candle_data(STOCK_SYMBOL);
            }
            candle_stick_create(ui_chart, STOCK_SYMBOL);
            lv_scr_load(ui_chart);
        } else {
            setupProvisioning(NULL, "LilyGo-StockTicker", NULL, false);
        }
    } else {
        setupProvisioning(NULL, "LilyGo-StockTicker", NULL, false);
    }

    pinMode(resetPin, INPUT_PULLUP);
}

void loop() {
    lv_task_handler();
    delay(5);

    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate > 1000) {
        updateTimeAndDate();
        lastTimeUpdate = millis();
    }

    if (digitalRead(resetPin) == LOW) {
        if (!resetButtonState) {
            resetButtonState = true;
            resetButtonPressedTime = millis();
        } else if (millis() - resetButtonPressedTime >= resetTime) {
            resetProvisioning();
        }
    } else {
        resetButtonState = false;
    }

    static unsigned long lastStockUpdate = 0;
    if (millis() - lastStockUpdate > (USE_INTRADAY_DATA ? INTRADAY_UPDATE_INTERVAL * 1000 : 12000)) {
        if (USE_INTRADAY_DATA) {
            update_intraday_data(STOCK_SYMBOL);
        } else if (USE_TEST_DATA || StockTracker::MarketHoursChecker::isMarketOpen()) {
            // Only fetch daily data if market is open or using test data
            if (fetch_candle_data(STOCK_SYMBOL)) {
                candle_stick_create(ui_chart, STOCK_SYMBOL);
            }
        }
        lastStockUpdate = millis();
    }
}