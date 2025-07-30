#include <Arduino.h>
#include <WiFi.h>
#include <LV_Helper.h>
#include <LilyGo_AMOLED.h>
#include <lvgl.h>
#include "ui.h"
#include "candle_stick.h"
#include "time_helper.h"
#include "config.h"
#include "market_hours.h"
#include "credentials.h"

LilyGo_Class amoled;

void connectWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected successfully");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection failed");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize display
    if (!amoled.begin()) {
        Serial.println("Display initialization failed!");
        while (1) delay(1000);
    }
    
    amoled.setRotation(0);
    amoled.setBrightness(125);
    
    // Initialize LVGL and UI
    beginLvglHelper(amoled);
    ui_init();
    
    // Load configuration
    loadConfig();
    
    // Initialize stock data parameters
    set_intraday_parameters(INTRADAY_UPDATE_INTERVAL, CANDLE_COLLECTION_DURATION);
    
    // Initialize test data if enabled
    if (USE_TEST_DATA) {
        initialize_test_data();
    }
    
    // Connect to WiFi
    connectWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        // Start NTP sync
        initiateNTPTimeSync();
        
        // Load chart screen
        lv_scr_load(ui_chart);
        
        // Fetch initial data and create chart
        if (USE_INTRADAY_DATA) {
            fetch_intraday_data(STOCK_SYMBOL);
        } else {
            fetch_candle_data(STOCK_SYMBOL);
        }
        candle_stick_create(ui_chart, STOCK_SYMBOL);
    } else {
        Serial.println("Cannot proceed without WiFi connection");
        // Could show error screen here
    }
}

void loop() {
    lv_task_handler();
    delay(5);
    
    // Update time display every second
    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate > 1000) {
        updateTimeAndDate();
        lastTimeUpdate = millis();
    }
    
    // Update stock data based on configuration
    static unsigned long lastStockUpdate = 0;
    unsigned long updateInterval = USE_INTRADAY_DATA ? 
                                  (INTRADAY_UPDATE_INTERVAL * 1000) : 
                                  (60 * 1000); // 60 seconds for daily data
    
    if (millis() - lastStockUpdate > updateInterval) {
        if (WiFi.status() == WL_CONNECTED) {
            if (USE_INTRADAY_DATA) {
                update_intraday_data(STOCK_SYMBOL);
            } else {
                if (fetch_candle_data(STOCK_SYMBOL)) {
                    candle_stick_create(ui_chart, STOCK_SYMBOL);
                }
            }
        }
        lastStockUpdate = millis();
    }
    
    // Check WiFi connection and reconnect if needed
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) { // Check every 30 seconds
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, attempting to reconnect...");
            connectWiFi();
        }
        lastWiFiCheck = millis();
    }
}