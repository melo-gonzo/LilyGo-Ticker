#include "config.h"
#include "credentials.h"
#include "data_fetcher.h"
#include "enhanced_candle_stick.h"
#include "market_hours.h"
#include "time_helper.h"
#include "ui.h"
#include "web_server.h"
#include <Arduino.h>
#include <LV_Helper.h>
#include <LilyGo_AMOLED.h>
#include <WiFi.h>
#include <lvgl.h>

LilyGo_Class amoled;

// State management
static bool data_needs_refresh = false;
static String last_symbol = "";
static String last_interval = "";
static String last_range = "";
static int last_bars_to_show = 0;

// Helper function to parse IP string to IPAddress
IPAddress parseIPAddress(const String &ipStr) {
  IPAddress ip;
  if (ip.fromString(ipStr)) {
    return ip;
  }
  // Return default IP if parsing fails
  return IPAddress(192, 168, 4, 184);
}

void connectWiFi() {
  Serial.println("Connecting to WiFi...");

  // Use configuration variables instead of hardcoded values
  if (USE_STATIC_IP) {
    Serial.println("Configuring static IP...");

    IPAddress local_IP = parseIPAddress(STATIC_IP);
    IPAddress gateway = parseIPAddress(GATEWAY_IP);
    IPAddress subnet = parseIPAddress(SUBNET_MASK);
    IPAddress primaryDNS(8, 8, 8, 8);   // Google DNS
    IPAddress secondaryDNS(8, 8, 4, 4); // Google DNS secondary

    Serial.println("Static IP: " + STATIC_IP);
    Serial.println("Gateway: " + GATEWAY_IP);
    Serial.println("Subnet: " + SUBNET_MASK);

    // Configure static IP
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Static IP configuration failed");
    } else {
      Serial.println("Static IP configuration successful");
    }
  } else {
    Serial.println("Using DHCP...");
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
    Serial.print("DNS 1: ");
    Serial.println(WiFi.dnsIP(0));
    Serial.print("DNS 2: ");
    Serial.println(WiFi.dnsIP(1));
  } else {
    Serial.println("WiFi connection failed");
  }
}

void checkConfigChanges() {
  // Track the previous test data state
  static bool last_use_test_data = USE_TEST_DATA;

  // Check if any critical parameters have changed
  bool config_changed = false;
  bool data_source_changed = false;

  if (last_symbol != STOCK_SYMBOL || last_interval != YAHOO_INTERVAL ||
      last_range != YAHOO_RANGE || last_bars_to_show != BARS_TO_SHOW ||
      last_use_test_data != USE_TEST_DATA) {

    config_changed = true;

    Serial.println("Configuration changed, refreshing display...");
    Serial.println("Symbol: " + last_symbol + " -> " + STOCK_SYMBOL);
    Serial.println("Interval: " + last_interval + " -> " + YAHOO_INTERVAL);
    Serial.println("Range: " + last_range + " -> " + YAHOO_RANGE);
    Serial.println("Bars: " + String(last_bars_to_show) + " -> " +
                   String(BARS_TO_SHOW));
    Serial.println("Use Test Data: " + String(last_use_test_data) + " -> " +
                   String(USE_TEST_DATA));

    // Check if data source changed (real data <-> test data)
    if (last_use_test_data != USE_TEST_DATA) {
      data_source_changed = true;
      Serial.println("Data source changed - will reset data fetcher");
    }

    // Refresh data if symbol, interval, range, or data source changed
    if (last_symbol != STOCK_SYMBOL || last_interval != YAHOO_INTERVAL ||
        last_range != YAHOO_RANGE || data_source_changed) {
      data_needs_refresh = true;
    }

    // Update tracked values
    last_symbol = STOCK_SYMBOL;
    last_interval = YAHOO_INTERVAL;
    last_range = YAHOO_RANGE;
    last_bars_to_show = BARS_TO_SHOW;
    last_use_test_data = USE_TEST_DATA;

    // Always refresh chart display for any config change
    EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
  }
}

void refreshDataIfNeeded() {
  if (data_needs_refresh) {
    Serial.println("Reinitializing data fetcher with new parameters...");

    // First, reset the data fetcher to clear any existing data
    DataFetcher::reset();
    Serial.println("Data fetcher reset - all existing data cleared");

    // Initialize data fetcher with new settings
    if (DataFetcher::initialize(STOCK_SYMBOL)) {
      Serial.println("Data fetcher reinitialized successfully");

      // Wait a moment for data to be processed
      delay(500);

      // Verify we have data
      if (DataFetcher::getCandleCount() > 0) {
        Serial.println("New data loaded successfully, creating chart with " +
                       String(DataFetcher::getCandleCount()) + " candles");
        // Force immediate chart update
        EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
      } else {
        Serial.println("Warning: No data loaded after reinitialization");
      }
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
    while (1)
      delay(1000);
  }

  amoled.setRotation(0);
  amoled.setBrightness(125);

  // Initialize LVGL and UI
  beginLvglHelper(amoled);
  ui_init();

  // Load configuration from preferences
  loadConfig();

  // printBarLimitations();

  // Ensure intraday data is always enabled for real-time updates
  if (!USE_INTRADAY_DATA) {
    USE_INTRADAY_DATA = true;
    saveConfig();
    Serial.println("Intraday data enabled for real-time updates");
  }

  // Store initial configuration state
  last_symbol = STOCK_SYMBOL;
  last_interval = YAHOO_INTERVAL;
  last_range = YAHOO_RANGE;
  last_bars_to_show = BARS_TO_SHOW;

  // Connect to WiFi using configuration
  connectWiFi();

  // Load chart screen first
  lv_scr_load(ui_chart);

  if (WiFi.status() == WL_CONNECTED) {
    // Start web server
    if (StockWebServer::begin()) {
      Serial.println("Web server started successfully");
    } else {
      Serial.println("Failed to start web server");
    }

    // Start NTP sync
    initiateNTPTimeSync();

    // Show loading message initially
    lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(ui_chart);
    if (chart_container != NULL) {
      lv_obj_t *loading_label = lv_label_create(chart_container);
      lv_label_set_text(loading_label, "Loading stock data...");
      lv_obj_center(loading_label);
      lv_obj_set_style_text_color(loading_label, lv_color_white(), 0);
    }

    // Initialize data fetcher and wait for data to load
    Serial.println("Initializing data fetcher...");
    if (DataFetcher::initialize(STOCK_SYMBOL)) {
      Serial.println("Data fetcher initialized successfully");

      // Wait a moment for data to be processed, then create chart
      delay(1000);

      // Verify we have data before creating chart
      int attempts = 0;
      while (DataFetcher::getCandleCount() == 0 && attempts < 10) {
        Serial.println("Waiting for data to load... attempt " +
                       String(attempts + 1));
        delay(1000);
        attempts++;
      }

      if (DataFetcher::getCandleCount() > 0) {
        Serial.println("Data loaded successfully, creating chart with " +
                       String(DataFetcher::getCandleCount()) + " candles");
        EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
      } else {
        Serial.println("No data loaded after retries, will retry in main loop");
        // Mark that we need to refresh data
        data_needs_refresh = true;
      }
    } else {
      Serial.println("Failed to initialize data fetcher");
      data_needs_refresh = true;
    }
  } else {
    Serial.println("Cannot proceed without WiFi connection");
    // Show error screen or message
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

  // ADDED: Initial data retry mechanism
  static bool initial_chart_created = false;
  static unsigned long lastInitialRetry = 0;

  if (!initial_chart_created && WiFi.status() == WL_CONNECTED) {
    if (DataFetcher::getCandleCount() > 0) {
      Serial.println("Initial chart creation - data now available");
      EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
      initial_chart_created = true;
    } else if (millis() - lastInitialRetry > 5000) { // Retry every 5 seconds
      Serial.println("Retrying initial data load...");
      if (DataFetcher::initialize(STOCK_SYMBOL)) {
        if (DataFetcher::getCandleCount() > 0) {
          EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
          initial_chart_created = true;
        }
      }
      lastInitialRetry = millis();
    }
  }

  // Update time display every second
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 1000) {
    updateTimeAndDate();
    lastTimeUpdate = millis();
  }

  // MODIFIED: Update stock data using millisecond intervals
  static unsigned long lastStockUpdate = 0;
  unsigned long updateInterval =
      INTRADAY_UPDATE_INTERVAL; // Now in milliseconds

  // Enforce minimum intervals based on data type
  if (!USE_TEST_DATA && updateInterval < 1000) {
    updateInterval = 1000; // Force minimum 1 second for real data
  }

  if (millis() - lastStockUpdate > updateInterval) {
    if (WiFi.status() == WL_CONNECTED && initial_chart_created) {
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

      // Reset initial chart flag to retry creation
      initial_chart_created = false;
    }
    lastWiFiCheck = millis();
  }

  // Periodic chart refresh (every 5 minutes) to ensure UI stays updated
  static unsigned long lastChartRefresh = 0;
  if (millis() - lastChartRefresh > 300000) { // 5 minutes
    if (WiFi.status() == WL_CONNECTED && initial_chart_created) {
      EnhancedCandleStick::update(ui_chart, STOCK_SYMBOL);
    }
    lastChartRefresh = millis();
  }
}