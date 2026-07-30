// main.cpp - LilyGo Ticker
//
// Boots the display, then the WiFi portal (saved networks in NVS, fallback AP
// for onboarding), then draws a candlestick chart of the configured symbol.
//
// Nothing in the loop blocks on the network coming up: if WiFi or NTP is not
// ready the chart shows its status and the loop keeps retrying, and if the
// station link drops the portal reconnects underneath (raising the fallback
// AP if the saved networks stay unreachable) without a reboot.

#include "config.h"
#include "data_fetcher.h"
#include "enhanced_candle_stick.h"
#include "market_hours.h"
#include "ota_update.h"
#include "time_helper.h"
#include "ui.h"
#include "web_server.h"
#include "wifi_portal.h"
#include <Arduino.h>
#include <LV_Helper.h>
#include <LilyGo_AMOLED.h>
#include <WiFi.h>
#include <lvgl.h>

LilyGo_Class amoled;

// State management
static bool data_needs_refresh = false;
static bool initial_chart_created = false;
static String last_symbol = "";
static String last_interval = "";
static String last_range = "";
static int last_bars_to_show = 0;

// Backoff for the initial load so a bad symbol or a Yahoo outage does not
// hammer the API every five seconds forever.
static unsigned long initial_retry_delay_ms = 5000;
static const unsigned long INITIAL_RETRY_MAX_MS = 120000;

// Replaces the chart contents with a single centred message. Used for the
// states that used to be dead ends (no WiFi, no clock, no data).
static void showMessage(const char *text, lv_color_t color) {
  lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(ui_chart);
  if (chart_container == NULL) {
    return;
  }
  lv_obj_clean(chart_container);
  lv_obj_t *label = lv_label_create(chart_container);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  lv_obj_set_style_text_color(label, color, 0);
}

void checkConfigChanges() {
  // Track the previous test data state
  static bool last_use_test_data = USE_TEST_DATA;

  bool data_source_changed = false;

  if (last_symbol != STOCK_SYMBOL || last_interval != YAHOO_INTERVAL ||
      last_range != YAHOO_RANGE || last_bars_to_show != BARS_TO_SHOW ||
      last_use_test_data != USE_TEST_DATA) {

    Serial.println("Configuration changed, refreshing display...");
    Serial.println("Symbol: " + last_symbol + " -> " + STOCK_SYMBOL);
    Serial.println("Interval: " + last_interval + " -> " + YAHOO_INTERVAL);
    Serial.println("Range: " + last_range + " -> " + YAHOO_RANGE);
    Serial.println("Bars: " + String(last_bars_to_show) + " -> " +
                   String(BARS_TO_SHOW));

    if (last_use_test_data != USE_TEST_DATA) {
      data_source_changed = true;
      Serial.println("Data source changed - will reset data fetcher");
    }

    // Refresh data if symbol, interval, range, or data source changed
    if (last_symbol != STOCK_SYMBOL || last_interval != YAHOO_INTERVAL ||
        last_range != YAHOO_RANGE || data_source_changed) {
      data_needs_refresh = true;
      initial_retry_delay_ms = 5000; // fresh parameters, retry promptly again
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
  if (!data_needs_refresh) {
    return;
  }
  if (!USE_TEST_DATA && (!wifiPortalIsConnected() || !isTimeSynchronized())) {
    return; // retried from the loop once the prerequisites are met
  }

  Serial.println("Reinitializing data fetcher with new parameters...");
  DataFetcher::reset();

  if (DataFetcher::initialize(STOCK_SYMBOL) &&
      DataFetcher::getCandleCount() > 0) {
    Serial.println("New data loaded, creating chart with " +
                   String(DataFetcher::getCandleCount()) + " candles");
    EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
    initial_chart_created = true;
    data_needs_refresh = false;
    initial_retry_delay_ms = 5000;
  } else {
    Serial.println("Reinitialization produced no data; will retry");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.printf("\nLilyGo Ticker starting (fw: %s)\n", FW_VERSION);

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

  // Timezone before any localtime use; the portal re-applies it after every
  // configTime() call, which would otherwise reset it to UTC.
  setenv("TZ", TIME_ZONE, 1);
  tzset();

  // Ensure intraday data is always enabled for real-time updates
  if (!USE_INTRADAY_DATA) {
    USE_INTRADAY_DATA = true;
    saveConfig();
  }

  // Store initial configuration state
  last_symbol = STOCK_SYMBOL;
  last_interval = YAHOO_INTERVAL;
  last_range = YAHOO_RANGE;
  last_bars_to_show = BARS_TO_SHOW;

  lv_scr_load(ui_chart);
  showMessage("Connecting to WiFi...", lv_color_white());
  lv_task_handler();

  // Saved networks first; falls back to its own AP if none are reachable, so
  // this never leaves the device stranded.
  wifiPortalConnect();

  // The web server runs on the AP too - that is the whole point of the
  // fallback - so it starts regardless of station state.
  StockWebServer::begin();
  otaInit();

  if (wifiPortalIsConnected()) {
    showMessage("Loading stock data...", lv_color_white());
    lv_task_handler();
    if (DataFetcher::initialize(STOCK_SYMBOL) &&
        DataFetcher::getCandleCount() > 0) {
      Serial.println("Data loaded, creating chart with " +
                     String(DataFetcher::getCandleCount()) + " candles");
      EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
      initial_chart_created = true;
    } else {
      Serial.println("No data yet, will retry from the main loop");
    }
  } else {
    showMessage("WiFi setup: join " AP_SSID "\nthen open http://192.168.8.1",
                lv_color_make(255, 140, 0));
  }
}

void loop() {
  lv_task_handler();
  delay(5);

  StockWebServer::handleClient();
  otaHandle();

  // Portal housekeeping: reconnects, pending joins, fallback AP.
  static unsigned long lastPortalTick = 0;
  if (millis() - lastPortalTick > 5000) {
    lastPortalTick = millis();
    wifiPortalTick();
  }

  // Check for configuration changes
  static unsigned long lastConfigCheck = 0;
  if (millis() - lastConfigCheck > 1000) {
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

  // An in-flight OTA transfer owns the radio; a Yahoo fetch alongside it
  // stalls the upload.
  if (otaInProgress()) {
    return;
  }

  bool ready = USE_TEST_DATA ||
               (wifiPortalIsConnected() && isTimeSynchronized());

  // Initial data retry, with backoff, once the prerequisites are met.
  static unsigned long lastInitialRetry = 0;
  if (!initial_chart_created && ready) {
    if (DataFetcher::getCandleCount() > 0) {
      EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
      initial_chart_created = true;
    } else if (millis() - lastInitialRetry > initial_retry_delay_ms) {
      lastInitialRetry = millis();
      Serial.printf("Retrying initial data load (next backoff %lus)...\n",
                    initial_retry_delay_ms / 1000);
      if (DataFetcher::initialize(STOCK_SYMBOL) &&
          DataFetcher::getCandleCount() > 0) {
        EnhancedCandleStick::create(ui_chart, STOCK_SYMBOL);
        initial_chart_created = true;
        initial_retry_delay_ms = 5000;
      } else {
        initial_retry_delay_ms =
            std::min(initial_retry_delay_ms * 2, INITIAL_RETRY_MAX_MS);
      }
    }
  }

  // Fetch new market data. Real data is floored well above the configurable
  // interval: each fetch is a TLS handshake plus a parse, and running that
  // once a second (the old default) starved the UI task and hammered Yahoo.
  static unsigned long lastStockUpdate = 0;
  unsigned long updateInterval = INTRADAY_UPDATE_INTERVAL;
  if (!USE_TEST_DATA && updateInterval < MIN_REAL_UPDATE_INTERVAL_MS) {
    updateInterval = MIN_REAL_UPDATE_INTERVAL_MS;
  }

  if (millis() - lastStockUpdate > updateInterval) {
    lastStockUpdate = millis();
    if (ready && initial_chart_created) {
      if (DataFetcher::updateData()) {
        EnhancedCandleStick::update(ui_chart, STOCK_SYMBOL);
      }
    }
  }

  // Periodic chart refresh so the market-open border and clock stay honest
  // even when no new bars arrive.
  static unsigned long lastChartRefresh = 0;
  if (millis() - lastChartRefresh > 300000) {
    lastChartRefresh = millis();
    if (initial_chart_created) {
      EnhancedCandleStick::update(ui_chart, STOCK_SYMBOL);
    }
  }
}
