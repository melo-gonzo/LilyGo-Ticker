#include <Arduino.h>
#include <WiFi.h>
#include <LV_Helper.h>
#include <LilyGo_AMOLED.h>
#include <lvgl.h>
#include "ui.h"
#include "enhanced_candle_stick.h"
#include "data_fetcher.h"
#include "time_helper.h"
#include "config.h"
#include "web_server.h"
#include "market_hours.h"
#include "credentials.h"

LilyGo_Class amoled;

// State management
static bool data_needs_refresh = false;
static String last_symbol = "";
static String last_interval = "";
static String last_range = "";
static int last_bars_to_show = 0;

void connectWiFi() {
    Serial.println("Connecting to WiFi...");
    
    // Configure static IP (modify these values for your network)
    IPAddress local_IP(192, 168, 4, 184);     // Set your desired static IP
    IPAddress gateway(192, 168, 4, 1);        // Your router's IP
    IPAddress subnet(255, 255, 255, 0);       // Subnet mask
    IPAddress primaryDNS(8, 8, 8, 8);         // Google DNS
    IPAddress secondaryDNS(8, 8, 4, 4);       // Google DNS secondary
    
    // Configure static IP
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Static IP configuration failed");
    }
    
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
        Serial.print("Gateway: ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("Subnet: ");
        Serial.println(WiFi.subnetMask());
    } else {
        Serial.println("WiFi connection failed");
    }
}

void checkConfigChanges() {
    // Check if any critical parameters have changed
    if (last_symbol != STOCK_SYMBOL || 
        last_interval != YAHOO_INTERVAL || 
        last_range != YAHOO_RANGE ||
        last_bars_to_show != BARS_TO_SHOW) {
        
        Serial.println("Configuration changed, refreshing display...");
        Serial.println("Symbol: " + last_symbol + " -> " + STOCK_SYMBOL);
        Serial.println("Interval: " + last_interval + " -> " + YAHOO_INTERVAL);
        Serial.println("Range: " + last_range + " -> " + YAHOO_RANGE);
        Serial.println("Bars: " + String(last_bars_to_show) + " -> " + String(BARS_TO_SHOW));
        
        // Only refresh data if symbol, interval, or range changed
        if (last_symbol != STOCK_SYMBOL || 
            last_interval != YAHOO_INTERVAL || 
            last_range != YAHOO_RANGE) {
            data_needs_refresh = true;
        }
        
        last_symbol = STOCK_SYMBOL;
        last_interval = YAHOO_INTERVAL;
        last_range = YAHOO_RANGE;
        last_bars_to_show = BARS_TO_SHOW;
        
        // Always refresh chart display for bars change
        EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
    }
}

void refreshDataIfNeeded() {
    if (data_needs_refresh) {
        Serial.println("Reinitializing data fetcher with new parameters...");
        
        // Initialize data fetcher with new settings
        if (DataFetcher::initialize(STOCK_SYMBOL)) {
            Serial.println("Data fetcher reinitialized successfully");
            // Force immediate chart update
            EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
        } else {
            Serial.println("Failed to reinitialize data fetcher");
        }
        
        data_needs_refresh = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Starting Stock Tracker...");
    
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
    
    // Load configuration from preferences
    loadConfig();
    
    // Store initial configuration state
    last_symbol = STOCK_SYMBOL;
    last_interval = YAHOO_INTERVAL;
    last_range = YAHOO_RANGE;
    last_bars_to_show = BARS_TO_SHOW;
    
    // Connect to WiFi
    connectWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        // Start web server
        if (StockWebServer::begin()) {
            Serial.println("Web server started successfully");
        } else {
            Serial.println("Failed to start web server");
        }
        
        // Start NTP sync
        initiateNTPTimeSync();
        
        // Load chart screen
        lv_scr_load(ui_chart);
        
        // Initialize data fetcher
        if (DataFetcher::initialize(STOCK_SYMBOL)) {
            Serial.println("Data fetcher initialized successfully");
            EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
        } else {
            Serial.println("Failed to initialize data fetcher");
        }
    } else {
        Serial.println("Cannot proceed without WiFi connection");
        // Show error screen or message
        lv_scr_load(ui_chart);
        lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(ui_chart);
        if (chart_container != NULL) {
            lv_obj_t *error_label = lv_label_create(chart_container);
            lv_label_set_text(error_label, "WiFi Connection Failed");
            lv_obj_center(error_label);
            lv_obj_set_style_text_color(error_label, lv_color_make(255, 0, 0), 0);
        }
    }
}

void loop() {
    lv_task_handler();
    delay(5);
    
    // Handle web server requests
    StockWebServer::handleClient();
    
    // Check for configuration changes
    static unsigned long lastConfigCheck = 0;
    if (millis() - lastConfigCheck > 1000) { // Check every second
        checkConfigChanges();
        refreshDataIfNeeded();
        lastConfigCheck = millis();
    }
    
    // Update time display every second
    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate > 1000) {
        updateTimeAndDate();
        lastTimeUpdate = millis();
    }
    
    // Update stock data based on configuration
    static unsigned long lastStockUpdate = 0;
    unsigned long updateInterval = INTRADAY_UPDATE_INTERVAL * 1000;
    
    if (millis() - lastStockUpdate > updateInterval) {
        if (WiFi.status() == WL_CONNECTED) {
            if (DataFetcher::updateData()) {
                // Only update chart if new data was fetched
                EnhancedCandleStick::update(ui_chart, STOCK_SYMBOL);
            }
        }
        lastStockUpdate = millis();
    }
    
    // Check WiFi connection and reconnect if needed
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) { // Check every 30 seconds
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, attempting to reconnect...");
            StockWebServer::stop(); // Stop web server during reconnection
            connectWiFi();
            
            // Restart web server if WiFi reconnected
            if (WiFi.status() == WL_CONNECTED) {
                StockWebServer::begin();
            }
        }
        lastWiFiCheck = millis();
    }
    
    // Periodic chart refresh (every 5 minutes) to ensure UI stays updated
    static unsigned long lastChartRefresh = 0;
    if (millis() - lastChartRefresh > 300000) { // 5 minutes
        if (WiFi.status() == WL_CONNECTED) {
            EnhancedCandleStick::update(ui_chart, STOCK_SYMBOL);
        }
        lastChartRefresh = millis();
    }
}