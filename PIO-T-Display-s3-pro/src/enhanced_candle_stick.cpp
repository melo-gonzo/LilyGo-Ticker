#include "enhanced_candle_stick.h"
#include "config.h"
#include "market_hours.h"
#include "ui.h"
#include <algorithm>

lv_obj_t *EnhancedCandleStick::find_obj_by_id(lv_obj_t *parent, uint32_t id) {
  if (parent == NULL)
    return NULL;

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

void EnhancedCandleStick::create(lv_obj_t *parent, const String &symbol) {
  lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(parent);
  if (chart_container == NULL) {
    return;
  }

  // Save existing info panel data
  lv_obj_t *saved_info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
  char time_text[16] = "";
  char date_text[16] = "";

  if (saved_info_panel != NULL) {
    lv_obj_t *time_label = find_obj_by_id(saved_info_panel, TIME_LABEL_ID);
    lv_obj_t *date_label = find_obj_by_id(saved_info_panel, DATE_LABEL_ID);

    if (time_label != NULL) {
      strncpy(time_text, lv_label_get_text(time_label), sizeof(time_text) - 1);
    }
    if (date_label != NULL) {
      strncpy(date_text, lv_label_get_text(date_label), sizeof(date_text) - 1);
    }
  }

  // Clean the container
  lv_obj_clean(chart_container);
  lv_obj_set_style_bg_color(chart_container, lv_color_black(), 0);

  // Get data from DataFetcher
  enhanced_candle_t *candles = DataFetcher::getCandles();
  int num_candles = DataFetcher::getCandleCount();
  int newest_index = DataFetcher::getNewestIndex();
  float current_price = DataFetcher::getCurrentPrice();

  // Debug output for bars calculation
  Serial.printf("=== BARS DISPLAY DEBUG ===\n");
  Serial.printf("BARS_TO_SHOW config: %d\n", BARS_TO_SHOW);
  Serial.printf("Available candles: %d\n", num_candles);
  Serial.printf("MAX_CANDLES limit: %d\n", MAX_CANDLES);
  Serial.printf("Newest index: %d\n", newest_index);

  if (num_candles == 0) {
    // No data available, show loading message
    lv_obj_t *loading_label = lv_label_create(chart_container);
    lv_label_set_text(loading_label, "Loading...");
    lv_obj_center(loading_label);
    lv_obj_set_style_text_color(loading_label, lv_color_white(), 0);
    Serial.println("No data available, showing loading message");
    return;
  }

  // Calculate how many bars to show and ensure it doesn't exceed available data
  int barsToShow = std::min(BARS_TO_SHOW, num_candles);
  Serial.printf("Actual bars to display: %d (limited by %s)\n", barsToShow,
                (barsToShow == BARS_TO_SHOW) ? "config" : "available data");

  // FIXED: Calculate price levels manually here instead of using the function
  float min_price = std::numeric_limits<float>::max();
  float max_price = std::numeric_limits<float>::lowest();

  Serial.printf("=== PRICE RANGE DEBUG ===\n");
  Serial.printf("Calculating price range for %d most recent bars:\n",
                barsToShow);

  // Get the most recent barsToShow candles
  for (int i = 0; i < barsToShow; i++) {
    // Get the i-th most recent candle
    int index = (newest_index - i + MAX_CANDLES) % MAX_CANDLES;
    const enhanced_candle_t &candle = candles[index];

    // // Debug first 5 and last 5 candles
    // if (i < 5 || i >= barsToShow - 5) {
    //     Serial.printf("  Bar %d: index=%d, high=%.2f, low=%.2f, open=%.2f,
    //     close=%.2f\n",
    //                  i, index, candle.high, candle.low, candle.open,
    //                  candle.close);
    // }

    // Only include candles with valid data
    if (candle.high > 0 && candle.low > 0 && candle.open > 0 &&
        candle.close > 0) {
      min_price = std::min(min_price, candle.low);
      max_price = std::max(max_price, candle.high);
    } else {
      Serial.printf("  WARNING: Invalid data at index %d\n", index);
    }
  }

  // Handle case where no valid data found
  if (min_price == std::numeric_limits<float>::max()) {
    Serial.println("ERROR: No valid price data found!");
    min_price = 0;
    max_price = 100;
  }

  // Serial.printf("Raw price range: %.2f - %.2f\n", min_price, max_price);

  // Add padding for drawing (10% padding)
  float range = max_price - min_price;
  if (range == 0)
    range = 1.0f; // Prevent division by zero
  float padding = range * 0.1f;
  float draw_min = std::max(min_price - padding, 0.0f);
  float draw_max = max_price + padding;

  // Serial.printf("Display range with padding: %.2f - %.2f\n", draw_min,
  // draw_max);

  // Draw grid lines with the visible range
  draw_price_gridlines(chart_container, draw_min, draw_max);

  // Draw candlesticks - display them in chronological order (oldest to newest,
  // left to right) Serial.printf("Drawing %d candlesticks in chronological
  // order:\n", barsToShow);
  for (int displayPos = 0; displayPos < barsToShow; displayPos++) {
    // Calculate which candle to show at this display position
    // displayPos 0 = oldest of the recent bars (newest_index - barsToShow + 1)
    // displayPos barsToShow-1 = newest bar (newest_index)
    int candleAge =
        barsToShow - displayPos - 1; // How many bars back from newest
    int dataIndex = (newest_index - candleAge + MAX_CANDLES) % MAX_CANDLES;

    // Debug first few bars
    if (displayPos < 3) {
      const enhanced_candle_t &debugCandle = candles[dataIndex];
      // Serial.printf("  Display pos %d: age=%d, dataIndex=%d, close=%.2f\n",
      //              displayPos, candleAge, dataIndex, debugCandle.close);
    }

    draw_candlestick(chart_container, displayPos, candles[dataIndex], draw_min,
                     draw_max, barsToShow);
  }

  // Draw current price line
  if (current_price > 0) {
    draw_current_price_line(chart_container, current_price, draw_min, draw_max);
  }

  // Create info panel with visible range min/max
  create_info_panel(chart_container, symbol, current_price, min_price,
                    max_price);

  // Restore time and date labels
  lv_obj_t *new_info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
  if (new_info_panel != NULL) {
    if (time_text[0] != '\0') {
      lv_obj_t *new_time_label = lv_label_create(new_info_panel);
      lv_label_set_text(new_time_label, time_text);
      lv_obj_align(new_time_label, LV_ALIGN_BOTTOM_MID, 0, -50);
      lv_obj_set_style_text_color(new_time_label, lv_color_white(), 0);
      lv_obj_set_user_data(new_time_label, (void *)TIME_LABEL_ID);
    }

    if (date_text[0] != '\0') {
      lv_obj_t *new_date_label = lv_label_create(new_info_panel);
      lv_label_set_text(new_date_label, date_text);
      lv_obj_align(new_date_label, LV_ALIGN_BOTTOM_MID, 0, -30);
      lv_obj_set_style_text_color(new_date_label, lv_color_white(), 0);
      lv_obj_set_user_data(new_date_label, (void *)DATE_LABEL_ID);
    }
  }

  // Add price labels on the left (using visible range)
  char min_price_str[16], max_price_str[16];
  snprintf(min_price_str, sizeof(min_price_str), "%.2f", min_price);
  snprintf(max_price_str, sizeof(max_price_str), "%.2f", max_price);

  lv_obj_t *min_label = lv_label_create(chart_container);
  lv_obj_t *max_label = lv_label_create(chart_container);

  lv_label_set_text(min_label, min_price_str);
  lv_label_set_text(max_label, max_price_str);

  lv_obj_align(min_label, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_obj_align(max_label, LV_ALIGN_TOP_LEFT, 5, 5);

  lv_obj_set_style_text_color(min_label, lv_color_white(), 0);
  lv_obj_set_style_text_color(max_label, lv_color_white(), 0);

  // Serial.printf("Chart creation complete. Displayed %d bars with range %.2f -
  // %.2f\n",
  //               barsToShow, min_price, max_price);
  Serial.printf("=========================\n");

  lv_task_handler();
}

void EnhancedCandleStick::update(lv_obj_t *parent, const String &symbol) {
  static bool market_closed_border = false;

  // Check market hours
  bool is_market_open = USE_TEST_DATA || !ENFORCE_MARKET_HOURS ||
                        StockTracker::MarketHoursChecker::isMarketOpen();

  // Update the chart
  create(parent, symbol);

  // Get the chart container for market status indication
  lv_obj_t *chart_container = (lv_obj_t *)lv_obj_get_user_data(parent);
  if (chart_container != NULL) {
    if (!is_market_open && !market_closed_border) {
      // Add orange border when market is closed
      lv_obj_set_style_border_width(chart_container, 3, 0);
      lv_obj_set_style_border_color(chart_container, lv_color_make(255, 140, 0),
                                    0);
      market_closed_border = true;

      // Add market closed status
      lv_obj_t *info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
      if (info_panel != NULL) {
        lv_obj_t *status_label = find_obj_by_id(info_panel, STATUS_LABEL_ID);
        if (status_label == NULL) {
          status_label = lv_label_create(info_panel);
          lv_obj_set_user_data(status_label, (void *)STATUS_LABEL_ID);
          lv_obj_set_style_text_color(status_label, lv_color_make(255, 140, 0),
                                      0);
          lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 120);
        }
        lv_label_set_text(status_label, "MARKET CLOSED");
      }
    } else if (is_market_open && market_closed_border) {
      // Remove border when market opens
      lv_obj_set_style_border_width(chart_container, 0, 0);
      market_closed_border = false;

      // Remove status label
      lv_obj_t *info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
      if (info_panel != NULL) {
        lv_obj_t *status_label = find_obj_by_id(info_panel, STATUS_LABEL_ID);
        if (status_label != NULL) {
          lv_obj_del(status_label);
        }
      }
    }

    // Update info panel with current configuration
    update_info_panel(chart_container, symbol);
  }
}

void EnhancedCandleStick::update_info_panel(lv_obj_t *chart_container,
                                            const String &symbol) {
  lv_obj_t *info_panel = find_obj_by_id(chart_container, INFO_PANEL_ID);
  if (info_panel == NULL)
    return;

  // Add interval information
  lv_obj_t *interval_label = find_obj_by_id(info_panel, INTERVAL_LABEL_ID);
  if (interval_label == NULL) {
    interval_label = lv_label_create(info_panel);
    lv_obj_set_user_data(interval_label, (void *)INTERVAL_LABEL_ID);
    lv_obj_set_style_text_color(interval_label, lv_color_make(200, 200, 200),
                                0);
    lv_obj_align(interval_label, LV_ALIGN_BOTTOM_MID, 0, -10);
  }

  String interval_text = YAHOO_INTERVAL + "/" + YAHOO_RANGE;
  lv_label_set_text(interval_label, interval_text.c_str());
}

void EnhancedCandleStick::draw_candlestick(lv_obj_t *parent, int index,
                                           const enhanced_candle_t &candle,
                                           float min_price, float max_price,
                                           int total_bars) {
  lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
  lv_coord_t chart_height = lv_obj_get_height(parent);

  // FIXED: Proper bar width calculation that allows 1-pixel candles
  int candle_width, spacing;

  if (CANDLE_PADDING == 0) {
    // No padding: divide available width evenly
    candle_width = chart_width / total_bars;
    spacing = 0;
  } else {
    // With padding: calculate optimal width and spacing
    // Available space = chart_width
    // Formula: total_bars * candle_width + (total_bars - 1) * padding =
    // chart_width Solve for candle_width: candle_width = (chart_width -
    // (total_bars - 1) * padding) / total_bars

    int total_padding_space = (total_bars - 1) * CANDLE_PADDING;
    candle_width = (chart_width - total_padding_space) / total_bars;
    spacing = CANDLE_PADDING;

    // If calculated width is too small, reduce padding
    if (candle_width < 1) {
      candle_width = 1;
      // Recalculate spacing with minimum 1-pixel candles
      int remaining_space = chart_width - total_bars;
      spacing = std::max(0, remaining_space / std::max(1, (total_bars - 1)));
    }
  }

  // Ensure minimum candle width of 1 pixel
  candle_width = std::max(1, candle_width);

  // Calculate position
  int x = index * (candle_width + spacing);

  // Debug output for first few candles
  if (index < 3) {
    Serial.printf(
        "Bar %d: width=%d, spacing=%d, x=%d, total_bars=%d, chart_width=%d\n",
        index, candle_width, spacing, x, total_bars, chart_width);
  }

  // Calculate Y positions
  int y_top = chart_height *
              (1.0f - (candle.high - min_price) / (max_price - min_price));
  int y_bottom = chart_height *
                 (1.0f - (candle.low - min_price) / (max_price - min_price));
  int y_open = chart_height *
               (1.0f - (candle.open - min_price) / (max_price - min_price));
  int y_close = chart_height *
                (1.0f - (candle.close - min_price) / (max_price - min_price));

  y_top = constrain(y_top, 0, chart_height);
  y_bottom = constrain(y_bottom, 0, chart_height);
  y_open = constrain(y_open, 0, chart_height);
  y_close = constrain(y_close, 0, chart_height);

  // SIMPLIFIED Color coding: ONLY green for up, red for down - NO ORANGE
  lv_color_t candle_color =
      (candle.close >= candle.open) ? lv_color_make(0, 255, 0) : // Green for up
          lv_color_make(255, 0, 0);                              // Red for down

  // Draw wick (always 1 pixel wide, centered)
  lv_obj_t *wick = lv_obj_create(parent);
  lv_obj_set_size(wick, 1, y_bottom - y_top);
  lv_obj_set_style_bg_color(wick, candle_color, 0);
  lv_obj_set_style_bg_opa(wick, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(wick, 0, 0);
  lv_obj_align(wick, LV_ALIGN_TOP_LEFT, x + (candle_width / 2), y_top);

  // Draw body
  lv_obj_t *body = lv_obj_create(parent);
  int body_y = (y_open < y_close) ? y_open : y_close;
  int body_height = abs(y_close - y_open);
  if (body_height < 1)
    body_height = 1; // Minimum height for doji candles

  lv_obj_set_size(body, candle_width, body_height);
  lv_obj_set_style_bg_color(body, candle_color, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_align(body, LV_ALIGN_TOP_LEFT, x, body_y);
}

void EnhancedCandleStick::draw_current_price_line(lv_obj_t *parent,
                                                  float current_price,
                                                  float min_price,
                                                  float max_price) {
  lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
  lv_coord_t chart_height = lv_obj_get_height(parent);

  int y_current = chart_height * (1.0f - (current_price - min_price) /
                                             (max_price - min_price));
  y_current = constrain(y_current, 0, chart_height);

  lv_obj_t *price_line = lv_obj_create(parent);
  lv_obj_set_size(price_line, chart_width, 2);
  lv_obj_set_style_bg_color(price_line, lv_color_make(0, 255, 255), 0);
  lv_obj_set_style_bg_opa(price_line, LV_OPA_70, 0);
  lv_obj_set_style_border_width(price_line, 0, 0);
  lv_obj_align(price_line, LV_ALIGN_TOP_LEFT, 0, y_current);
}

void EnhancedCandleStick::draw_price_gridlines(lv_obj_t *parent,
                                               float min_price,
                                               float max_price) {
  lv_coord_t chart_width = lv_obj_get_width(parent) - INFO_PANEL_WIDTH;
  lv_coord_t chart_height = lv_obj_get_height(parent);

  // FIXED: Use a more intelligent grid calculation
  float price_range = max_price - min_price;
  float grid_interval;

  // Calculate appropriate grid interval based on price range
  if (price_range > 100) {
    grid_interval = 10.0f;
  } else if (price_range > 50) {
    grid_interval = 5.0f;
  } else if (price_range > 10) {
    grid_interval = 1.0f;
  } else if (price_range > 1) {
    grid_interval = 0.5f;
  } else {
    grid_interval = 0.1f;
  }

  // Find first grid line
  float first_line = ceil(min_price / grid_interval) * grid_interval;

  lv_color_t grid_color = lv_color_make(100, 100, 100);

  for (float price = first_line; price <= max_price; price += grid_interval) {
    int y_pos =
        chart_height * (1.0f - (price - min_price) / (max_price - min_price));
    y_pos = constrain(y_pos, 0, chart_height);

    lv_obj_t *grid_line = lv_obj_create(parent);
    lv_obj_set_size(grid_line, chart_width, 1);
    lv_obj_set_style_bg_color(grid_line, grid_color, 0);
    lv_obj_set_style_bg_opa(grid_line, LV_OPA_50, 0);
    lv_obj_set_style_border_width(grid_line, 0, 0);
    lv_obj_align(grid_line, LV_ALIGN_TOP_LEFT, 0, y_pos);
  }
}

void EnhancedCandleStick::create_info_panel(lv_obj_t *parent,
                                            const String &symbol,
                                            float current_price,
                                            float min_price, float max_price) {
  lv_coord_t chart_height = lv_obj_get_height(parent);

  lv_obj_t *info_panel = lv_obj_create(parent);
  lv_obj_set_size(info_panel, INFO_PANEL_WIDTH, chart_height);
  lv_obj_align(info_panel, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(info_panel, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(info_panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(info_panel, 0, 0);
  lv_obj_set_user_data(info_panel, (void *)INFO_PANEL_ID);

  // Symbol label
  lv_obj_t *symbol_label = lv_label_create(info_panel);
  lv_label_set_text(symbol_label, symbol.c_str());
  lv_obj_align(symbol_label, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(symbol_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(symbol_label, &lv_font_montserrat_16, 0);
  lv_obj_set_user_data(symbol_label, (void *)SYMBOL_LABEL_ID);

  // Current price label
  lv_obj_t *price_label = lv_label_create(info_panel);
  char price_buf[16];
  snprintf(price_buf, sizeof(price_buf), "%.2f", current_price);
  lv_label_set_text(price_label, price_buf);
  lv_obj_align(price_label, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_style_text_color(price_label, lv_color_make(0, 255, 255), 0);
  lv_obj_set_style_text_font(price_label, &lv_font_montserrat_16, 0);
  lv_obj_set_user_data(price_label, (void *)PRICE_LABEL_ID);

  // High/Low labels (using visible range)
  lv_obj_t *high_label = lv_label_create(info_panel);
  lv_obj_t *low_label = lv_label_create(info_panel);

  char high_buf[32], low_buf[32];
  snprintf(high_buf, sizeof(high_buf), "H: %.2f", max_price);
  snprintf(low_buf, sizeof(low_buf), "L: %.2f", min_price);

  lv_label_set_text(high_label, high_buf);
  lv_label_set_text(low_label, low_buf);

  lv_obj_align(high_label, LV_ALIGN_TOP_MID, 0, 75);
  lv_obj_align(low_label, LV_ALIGN_TOP_MID, 0, 95);

  lv_obj_set_style_text_color(high_label, lv_color_make(0, 255, 0), 0);
  lv_obj_set_style_text_color(low_label, lv_color_make(255, 0, 0), 0);

  lv_obj_set_user_data(high_label, (void *)HIGH_LABEL_ID);
  lv_obj_set_user_data(low_label, (void *)LOW_LABEL_ID);
}