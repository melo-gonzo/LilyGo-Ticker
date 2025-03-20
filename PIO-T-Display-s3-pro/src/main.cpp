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
        initialize_test_data();  // Initialize with historical test data
    } else {
        set_intraday_parameters(INTRADAY_UPDATE_INTERVAL, CANDLE_COLLECTION_DURATION);
    }

    if (isProvisioned()) {
        WiFi.begin();
        Serial.println("Connecting to WiFi...");

        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
            lv_task_handler();
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected. Starting NTP sync...");
            initiateNTPTimeSync();

            // Load chart screen before fetching data
            lv_scr_load(ui_chart);

            // Fetch data and create chart
            if (USE_INTRADAY_DATA) {
                fetch_intraday_data(STOCK_SYMBOL);
            } else {
                fetch_candle_data(STOCK_SYMBOL);
            }
            candle_stick_create(ui_chart, STOCK_SYMBOL);
        } else {
            Serial.println("WiFi connection failed. Starting provisioning...");
            setupProvisioning(NULL, "LilyGo-StockTicker", NULL, false);
        }
    } else {
        Serial.println("Not provisioned. Starting WiFi provisioning...");
        setupProvisioning(NULL, "LilyGo-StockTicker", NULL, false);
    }

    pinMode(resetPin, INPUT_PULLUP);
}

void loop() {
    lv_task_handler();
    delay(5);

    // Handle reset button
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

    // Create a dedicated time update loop - only update the labels, not redraw the chart
    static unsigned long lastTimeOnlyUpdate = 0;
    if (millis() - lastTimeOnlyUpdate > 1000) {  // Update every second
        updateTimeAndDate();  // Using our improved version that only changes text when needed
        lastTimeOnlyUpdate = millis();
    }

    // Separate stock chart update loop - don't mix with time updates
    static unsigned long lastStockUpdate = 0;
    unsigned long updateInterval = USE_INTRADAY_DATA ? 
                                  (INTRADAY_UPDATE_INTERVAL * 1000) : 
                                  (60 * 1000); // Use 60 seconds for daily data

    if (millis() - lastStockUpdate > updateInterval) {
        if (USE_INTRADAY_DATA) {
            update_intraday_data(STOCK_SYMBOL); // This calls candle_stick_create
        } else {
            // Always fetch daily data, regardless of market hours
            if (fetch_candle_data(STOCK_SYMBOL)) {
                candle_stick_create(ui_chart, STOCK_SYMBOL);
            }
        }
        lastStockUpdate = millis();
    }
}