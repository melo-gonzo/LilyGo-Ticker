#include "data_fetcher.h"
#include "market_hours.h"
#include <WiFiClientSecure.h>
#include <algorithm>

// Static member definitions
enhanced_candle_t DataFetcher::candles[MAX_CANDLES];
int DataFetcher::newest_candle_index = -1;
int DataFetcher::num_candles = 0;
time_t DataFetcher::last_update_time = 0;
float DataFetcher::current_price = 0.0;
bool DataFetcher::initial_data_loaded = false;
String DataFetcher::current_symbol = "";
String DataFetcher::current_interval = "";
String DataFetcher::current_range = "";
unsigned long DataFetcher::consecutive_failures = 0;
time_t DataFetcher::last_success_time = 0;

// Any epoch past this means NTP has actually run (Nov 2023).
static const time_t CLOCK_SYNCED_AFTER = 1700000000;

bool DataFetcher::initialize(const String &symbol) {
  // Always reset first to ensure clean state
  reset();

  current_symbol = symbol;
  current_interval = YAHOO_INTERVAL;
  current_range = YAHOO_RANGE;

  Serial.println("Initializing DataFetcher for symbol: " + symbol);
  Serial.println("Use test data: " + String(USE_TEST_DATA));

  if (USE_TEST_DATA) {
    Serial.println("Using test data mode");
    initializeTestData();
    return true;
  }

  Serial.println("Using real data mode");
  return fetchInitialData(symbol, YAHOO_INTERVAL, YAHOO_RANGE);
}

// ---------------- request sizing ----------------

int DataFetcher::getIntervalSeconds(const String &interval) {
  if (interval == "1m")
    return 60;
  if (interval == "2m")
    return 120;
  if (interval == "5m")
    return 300;
  if (interval == "15m")
    return 900;
  if (interval == "30m")
    return 1800;
  if (interval == "60m" || interval == "1h")
    return 3600;
  if (interval == "90m")
    return 5400;
  if (interval == "1d")
    return 86400;
  if (interval == "5d")
    return 432000;
  if (interval == "1wk")
    return 604800;
  if (interval == "1mo")
    return 2592000;
  if (interval == "3mo")
    return 7776000;

  // Default to the configured candle collection duration
  return CANDLE_COLLECTION_DURATION;
}

// Calendar seconds spanned by a Yahoo range string.
static long rangeToSeconds(const String &range) {
  const long DAY = 86400L;
  if (range == "1d")
    return DAY;
  if (range == "5d")
    return 5 * DAY;
  if (range == "1mo")
    return 31 * DAY;
  if (range == "3mo")
    return 93 * DAY;
  if (range == "6mo")
    return 186 * DAY;
  if (range == "1y")
    return 366 * DAY;
  if (range == "2y")
    return 731 * DAY;
  if (range == "5y")
    return 1827 * DAY;
  if (range == "10y")
    return 3653 * DAY;
  if (range == "ytd") {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return (long)ti.tm_yday * DAY + ti.tm_hour * 3600L + DAY;
  }
  // "max" and anything unrecognised: let the bar-count cap decide.
  return 40L * 366 * DAY;
}

// How far back Yahoo will serve a windowed request for a given interval.
// Asking for more returns an error rather than a truncated series.
static long maxLookbackForInterval(const String &interval) {
  const long DAY = 86400L;
  if (interval == "1m")
    return 7 * DAY;
  if (interval == "2m" || interval == "5m" || interval == "15m" ||
      interval == "30m" || interval == "90m")
    return 59 * DAY;
  if (interval == "60m" || interval == "1h")
    return 729 * DAY;
  return 40L * 366 * DAY; // daily and coarser: effectively unbounded
}

long DataFetcher::lookbackSeconds(const String &interval, const String &range,
                                  int maxBars) {
  long ivl = getIntervalSeconds(interval);

  // Fraction of wall-clock time that actually produces bars. Regular
  // sessions are 6.5h on 5 weekdays, so intraday intervals only fill ~19% of
  // the calendar; daily-and-coarser bars fill 5 days in 7.
  float duty = (ivl < 86400) ? (6.5f / 24.0f) * (5.0f / 7.0f) : (5.0f / 7.0f);

  // Never ask for more bars than the ring buffer can hold. This is what
  // keeps the response bounded no matter which range the web UI selects -
  // the old code requested the full range and fell over above 50KB.
  long capacity = (long)((double)maxBars * ivl / duty);
  long span = std::min(rangeToSeconds(range), capacity);
  span = std::min(span, maxLookbackForInterval(interval));
  return std::max(span, (long)ivl * 2);
}

// ---------------- fetching ----------------

bool DataFetcher::fetchWindow(const String &symbol, const String &interval,
                              time_t period1, time_t period2, bool keepPartial,
                              std::vector<enhanced_candle_t> &out) {
  out.clear();
  if (period1 < 0)
    period1 = 0;

  WiFiClientSecure client;
  client.setInsecure(); // Yahoo's cert rotates; we only read public quotes
  client.setTimeout(15);

  String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + symbol +
               "?interval=" + interval +
               "&period1=" + String((unsigned long)period1) +
               "&period2=" + String((unsigned long)period2) +
               "&includePrePost=false";

  HTTPClient http;
  http.useHTTP10(true); // no chunked encoding -> stream straight into parser
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    Serial.println("[yahoo] http.begin failed");
    return false;
  }
  // Yahoo rejects requests without a browser-ish agent.
  http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; LilyGoTicker)");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[yahoo] HTTP %d for %s %s\n", code, symbol.c_str(),
                  interval.c_str());
    http.end();
    return false;
  }

  // Keep only the arrays we need; meta and everything else is discarded as
  // the response streams through the parser, so peak RAM tracks the bar
  // count rather than the payload size.
  JsonDocument filter;
  filter["chart"]["result"][0]["timestamp"] = true;
  filter["chart"]["result"][0]["indicators"]["quote"][0] = true;
  filter["chart"]["error"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("[yahoo] JSON parse failed: %s\n", err.c_str());
    return false;
  }
  if (!doc["chart"]["error"].isNull()) {
    Serial.println("[yahoo] API returned an error (bad symbol or range?)");
    return false;
  }

  JsonObject result = doc["chart"]["result"][0];
  JsonArray ts = result["timestamp"];
  if (ts.isNull()) {
    return true; // valid response, no trading in this window
  }
  JsonObject quote = result["indicators"]["quote"][0];
  JsonArray opens = quote["open"];
  JsonArray highs = quote["high"];
  JsonArray lows = quote["low"];
  JsonArray closes = quote["close"];
  JsonArray volumes = quote["volume"];

  int intervalSec = getIntervalSeconds(interval);
  time_t now = time(nullptr);
  out.reserve(ts.size() < MAX_CANDLES ? ts.size() : MAX_CANDLES);

  // Grid anchor for the phantom-bar filter below. Real bars all share one
  // offset (intraday sessions open at 09:30 ET, so e.g. 60m bars sit at
  // :30 past the hour, not on the epoch hour) - so alignment is checked
  // against the first bar of this response, not against the epoch.
  time_t anchor = -1;
  for (size_t i = 0; i < ts.size(); i++) {
    time_t t = ts[i].as<long>();
    if (t % 60 == 0) {
      anchor = t;
      break;
    }
  }

  for (size_t i = 0; i < ts.size(); i++) {
    if (closes[i].isNull())
      continue; // halted or empty period
    time_t start = ts[i].as<long>();
    if (start <= period1)
      continue; // Yahoo degrades the bar sitting exactly at period1; callers
                // over-reach the window so nothing real is lost

    // Yahoo appends a phantom bar to every windowed response: one entry at
    // meta.regularMarketTime carrying O=H=L=C=last price. Its timestamp has
    // second resolution and moves with every request, so the merge below
    // reads it as a brand new bar and appends a flat doji on every fetch
    // cycle instead of updating the bar in progress. Real bars always start
    // on a whole minute and on the series' own interval grid.
    if (intervalSec < 86400) {
      if (start % 60 != 0)
        continue;
      if (anchor >= 0 && ((start - anchor) % intervalSec) != 0)
        continue;
    }
    // A bar cannot have started in the future. period2 over-reaches by one
    // interval to pick up the bar in progress, which must not be read as
    // licence to accept a bar beyond it.
    if (now > CLOCK_SYNCED_AFTER && start > now)
      continue;

    enhanced_candle_t candle;
    candle.timestamp = start;
    candle.close = closes[i].as<float>();
    candle.open = opens[i].isNull() ? candle.close : opens[i].as<float>();
    candle.high = highs[i].isNull() ? candle.close : highs[i].as<float>();
    candle.low = lows[i].isNull() ? candle.close : lows[i].as<float>();
    candle.volume = volumes[i].isNull() ? 0 : volumes[i].as<uint64_t>();
    candle.is_complete = (start + intervalSec) <= now;

    if (!candle.is_complete && !keepPartial)
      continue;
    if (!validateCandle(candle))
      continue;

    out.push_back(candle);
  }

  // Defensive: the sizing math should keep us under the cap, but a change in
  // Yahoo's session coverage shouldn't be able to overrun the ring buffer.
  if ((int)out.size() > MAX_CANDLES) {
    out.erase(out.begin(), out.end() - MAX_CANDLES);
  }
  return true;
}

bool DataFetcher::fetchInitialData(const String &symbol, const String &interval,
                                   const String &range) {
  time_t now = time(nullptr);
  if (now < CLOCK_SYNCED_AFTER) {
    Serial.println("Clock not synced yet, deferring initial fetch");
    return false;
  }

  long lookback = lookbackSeconds(interval, range, MAX_CANDLES);
  int intervalSec = getIntervalSeconds(interval);

  Serial.printf("Fetching %s %s: %ld s of history (range=%s, capped to "
                "%d bars)\n",
                symbol.c_str(), interval.c_str(), lookback, range.c_str(),
                MAX_CANDLES);

  std::vector<enhanced_candle_t> bars;
  // period2 reaches one interval past now so the in-progress bar is included
  // (Yahoo drops bars that extend beyond period2).
  if (!fetchWindow(symbol, interval, now - lookback, now + intervalSec, true,
                   bars)) {
    consecutive_failures++;
    return false;
  }
  if (bars.empty()) {
    Serial.println("No bars returned for the requested window");
    consecutive_failures++;
    return false;
  }

  reset();
  current_symbol = symbol;
  current_interval = interval;
  current_range = range;

  for (const enhanced_candle_t &bar : bars) {
    updateCircularBuffer(bar);
  }

  current_price = candles[newest_candle_index].close;
  initial_data_loaded = true;
  consecutive_failures = 0;
  last_success_time = time(nullptr);
  last_update_time = last_success_time;

  Serial.println("Loaded " + String(num_candles) +
                 " candles. Current price: " + String(current_price));
  return true;
}

// Inserts a bar, replacing the entry with the same timestamp if present
// (in-progress candle, late revision) and appending only when it is newer
// than everything held. Out-of-order gap fills are dropped rather than
// appended, which would scramble the ring buffer's chronological order.
// Returns true only when the buffer actually changed, so a fetch that adds
// nothing new does not trigger a full chart rebuild.
bool DataFetcher::upsertCandle(const enhanced_candle_t &candle,
                               int intervalSec) {
  for (int age = 0; age < num_candles; age++) {
    int idx = (newest_candle_index - age + MAX_CANDLES) % MAX_CANDLES;
    if (candles[idx].timestamp == candle.timestamp) {
      enhanced_candle_t merged = candle;
      // Yahoo reports the first bar of a windowed response with zero volume.
      // The live window slides, so every bar eventually takes a turn as the
      // first one - left alone, that wipes the volume of the whole session a
      // bar at a time. A zero never overwrites a figure we already have.
      if (merged.volume == 0) {
        merged.volume = candles[idx].volume;
      }
      const enhanced_candle_t &held = candles[idx];
      bool same = held.open == merged.open && held.high == merged.high &&
                  held.low == merged.low && held.close == merged.close &&
                  held.volume == merged.volume &&
                  held.is_complete == merged.is_complete;
      candles[idx] = merged;
      return !same;
    }
    if (candles[idx].timestamp < candle.timestamp)
      break; // bars are ordered; nothing further back can match
  }
  if (num_candles > 0) {
    time_t newest = candles[newest_candle_index].timestamp;
    if (candle.timestamp <= newest) {
      return false;
    }
    // Consecutive bars of an intraday series sit exactly one interval apart
    // on the same grid, so anything closer than that is not a new bar - it
    // is a mid-bar artifact (Yahoo's regularMarketTime entry is the one that
    // bites) that would otherwise be appended as a flat doji. Daily and
    // coarser bars are excluded: calendar months are shorter than the
    // nominal interval, so the gap test does not hold there.
    if (intervalSec < 86400 && (candle.timestamp - newest) < intervalSec) {
      return false;
    }
  }
  updateCircularBuffer(candle);
  return true;
}

bool DataFetcher::updateData() {
  if (USE_TEST_DATA) {
    // TEST DATA MODE: one synthetic tick per call; the caller owns cadence.
    time_t now;
    time(&now);
    float price = getRandomPrice();

    int candles_before = num_candles;
    bool was_complete =
        (num_candles > 0) ? candles[newest_candle_index].is_complete : false;

    buildIntradayCandle(price, now);

    bool is_complete =
        (num_candles > 0) ? candles[newest_candle_index].is_complete : false;

    static unsigned long lastTestDebug = 0;
    if (millis() - lastTestDebug > 3000) {
      Serial.printf("[test] price %.2f, %d candles, current %s\n", price,
                    num_candles, is_complete ? "COMPLETE" : "BUILDING");
      if (num_candles > candles_before)
        Serial.println("[test] new candle created");
      if (!was_complete && is_complete)
        Serial.println("[test] candle completed");
      lastTestDebug = millis();
    }
    return true;
  }

  // REAL DATA MODE
  if (!initial_data_loaded) {
    return false; // nothing to merge into yet; main loop retries the load
  }

  time_t now = time(nullptr);
  if (now < CLOCK_SYNCED_AFTER) {
    return false;
  }

  // Outside the collection window there are no new bars to fetch. The grace
  // period keeps fetching briefly past the bell so the session's last candle
  // - which only completes after the close - still lands.
  if (!StockTracker::MarketHoursChecker::isCollectionWindow(5)) {
    return false;
  }

  // Trailing window: a few intervals back so a brief outage or a late
  // revision self-heals, over-reaching period1 because the bar sitting there
  // is dropped. period2 runs one interval past now to include the bar in
  // progress.
  int intervalSec = getIntervalSeconds(current_interval);
  long window = std::max((long)intervalSec * 4, 600L);

  std::vector<enhanced_candle_t> bars;
  if (!fetchWindow(current_symbol, current_interval, now - window,
                   now + intervalSec, true, bars)) {
    consecutive_failures++;
    Serial.printf("Live fetch failed (%lu in a row)\n", consecutive_failures);
    return false;
  }

  consecutive_failures = 0;
  last_success_time = now;
  last_update_time = now;

  if (bars.empty()) {
    return false; // window held no trading; nothing to redraw
  }

  bool changed = false;
  for (const enhanced_candle_t &bar : bars) {
    changed |= upsertCandle(bar, intervalSec);
  }
  current_price = candles[newest_candle_index].close;

  // Redrawing means tearing down and rebuilding one LVGL object per candle,
  // so it only happens when a bar actually moved.
  if (!changed) {
    return false;
  }

  const enhanced_candle_t &newest = candles[newest_candle_index];
  Serial.printf("%s O %.2f H %.2f L %.2f C %.2f V %llu %s\n",
                current_symbol.c_str(), newest.open, newest.high, newest.low,
                newest.close, (unsigned long long)newest.volume,
                newest.is_complete ? "" : "(building)");
  return true;
}

void DataFetcher::buildIntradayCandle(float price, time_t timestamp) {
  static int update_count = 0;
  static int last_candle_count = 0; // Track if data was reset

  // Reset update counter if data was cleared
  if (num_candles < last_candle_count || num_candles == 0) {
    update_count = 0;
  }
  last_candle_count = num_candles;

  // Only test mode synthesises candles from a price stream; real bars come
  // from Yahoo's own OHLC via fetchWindow/upsertCandle.
  update_count++;

  if (num_candles == 0) {
    enhanced_candle_t newCandle;
    newCandle.timestamp = timestamp;
    newCandle.open = price;
    newCandle.high = price;
    newCandle.low = price;
    newCandle.close = price;
    newCandle.volume = random(200000, 1200000);
    newCandle.is_complete = false;

    updateCircularBuffer(newCandle);
  } else if (candles[newest_candle_index].is_complete) {
    // Previous candle was completed, start a new one
    enhanced_candle_t newCandle;
    newCandle.timestamp = timestamp;
    newCandle.open = price;
    newCandle.high = price;
    newCandle.low = price;
    newCandle.close = price;
    newCandle.volume = random(200000, 1200000);
    newCandle.is_complete = false;

    updateCircularBuffer(newCandle);
    update_count = 1;
  } else {
    candles[newest_candle_index].close = price;
    candles[newest_candle_index].high =
        std::max(candles[newest_candle_index].high, price);
    candles[newest_candle_index].low =
        std::min(candles[newest_candle_index].low, price);
    candles[newest_candle_index].timestamp = timestamp;
    candles[newest_candle_index].volume += random(20000, 120000);

    if (update_count >= TEST_DATA_UPDATES_PER_BAR) {
      candles[newest_candle_index].is_complete = true;
    }
  }

  current_price = price;
}

bool DataFetcher::shouldCreateNewCandle(time_t current_time,
                                        time_t last_candle_time,
                                        int interval_seconds) {
  return (current_time / interval_seconds) !=
         (last_candle_time / interval_seconds);
}

void DataFetcher::updateCircularBuffer(const enhanced_candle_t &candle) {
  if (num_candles < MAX_CANDLES) {
    num_candles++;
  }

  newest_candle_index = (newest_candle_index + 1) % MAX_CANDLES;
  candles[newest_candle_index] = candle;
}

void DataFetcher::getPriceLevels(float *min_price, float *max_price) {
  *min_price = std::numeric_limits<float>::max();
  *max_price = std::numeric_limits<float>::lowest();

  if (num_candles == 0) {
    *min_price = 0;
    *max_price = 100;
    return;
  }

  for (int i = 0; i < num_candles; i++) {
    int index =
        (newest_candle_index - num_candles + i + 1 + MAX_CANDLES) % MAX_CANDLES;
    *min_price = std::min(*min_price, candles[index].low);
    *max_price = std::max(*max_price, candles[index].high);
  }

  // Round to 2 decimal places
  *min_price = floor(*min_price * 100) / 100;
  *max_price = ceil(*max_price * 100) / 100;
}

void DataFetcher::getPriceLevelsForVisibleBars(float *min_price,
                                               float *max_price,
                                               int bars_to_show) {
  *min_price = std::numeric_limits<float>::max();
  *max_price = std::numeric_limits<float>::lowest();

  if (num_candles == 0) {
    *min_price = 0;
    *max_price = 100;
    return;
  }

  // Limit bars_to_show to available data
  int barsToCheck = std::min(bars_to_show, num_candles);

  // Calculate min/max for only the MOST RECENT visible bars
  for (int i = 0; i < barsToCheck; i++) {
    int index = (newest_candle_index - i + MAX_CANDLES) % MAX_CANDLES;
    const enhanced_candle_t &candle = candles[index];

    if (candle.high > 0 && candle.low > 0 && candle.open > 0 &&
        candle.close > 0) {
      *min_price = std::min(*min_price, candle.low);
      *max_price = std::max(*max_price, candle.high);
    }
  }

  // If we didn't find any valid data, use fallback
  if (*min_price == std::numeric_limits<float>::max()) {
    Serial.println("  No valid price data found, using fallback");
    *min_price = 0;
    *max_price = 100;
    return;
  }

  // Round to 2 decimal places
  *min_price = floor(*min_price * 100) / 100;
  *max_price = ceil(*max_price * 100) / 100;
}

float DataFetcher::getRandomPrice() {
  static float lastPrice = 250.0;
  static bool priceInitialized = false;
  static int lastCandleCount = 0; // Track if data was reset

  // Reset price initialization if data was cleared
  if (num_candles < lastCandleCount) {
    priceInitialized = false;
    lastPrice = 250.0;
    Serial.println("Test data: Price generator reset due to data clear");
  }
  lastCandleCount = num_candles;

  // If we have candles in the buffer, use the most recent close price
  if (!priceInitialized && num_candles > 0) {
    lastPrice = candles[newest_candle_index].close;
    priceInitialized = true;
  }

  // Generate realistic price movement (-1% to +1% change)
  float changePercent = (random(-100, 101) / 10000.0); // -0.01 to +0.01
  float change = lastPrice * changePercent;
  lastPrice += change;

  // Ensure price doesn't go negative
  lastPrice = std::max(0.01f, lastPrice);

  return lastPrice;
}

void DataFetcher::initializeTestData() {
  Serial.println("Initializing realistic test data...");
  Serial.println("Using " + String(TEST_DATA_UPDATES_PER_BAR) +
                 " updates per candle for realistic OHLC generation");

  float base_price = 250.0;
  time_t now;
  time(&now);

  // IMPORTANT: Reset the buffer first to clear any existing real data
  reset();

  // Pre-populate with historical test data
  int test_candles = std::min(MAX_CANDLES - 5, 500);

  for (int i = 0; i < test_candles; i++) {
    enhanced_candle_t candle;

    float starting_price = base_price;
    candle.open = starting_price;
    candle.high = starting_price;
    candle.low = starting_price;
    candle.close = starting_price;

    // Simulate TEST_DATA_UPDATES_PER_BAR price updates to build this candle
    float current = starting_price;
    for (int update = 0; update < TEST_DATA_UPDATES_PER_BAR; update++) {
      float change_percent = (random(-50, 51) / 10000.0); // -0.5% to +0.5%
      current += current * change_percent;
      current = std::max(0.01f, current);

      candle.high = std::max(candle.high, current);
      candle.low = std::min(candle.low, current);
      candle.close = current;
    }

    // Ensure low is never higher than open/close and high is never lower
    candle.low = std::min({candle.low, candle.open, candle.close});
    candle.high = std::max({candle.high, candle.open, candle.close});
    candle.volume = random(200000, 1200000);

    candle.timestamp = now - (test_candles - i) * CANDLE_COLLECTION_DURATION;
    candle.is_complete = true;

    updateCircularBuffer(candle);
    base_price = candle.close;
  }

  current_price = candles[newest_candle_index].close;
  initial_data_loaded = true;
  last_success_time = now;

  Serial.println("Generated " + String(num_candles) +
                 " test candles, final price " + String(current_price));
}

bool DataFetcher::validateCandle(const enhanced_candle_t &candle) {
  if (candle.open <= 0 || candle.close <= 0 || candle.high <= 0 ||
      candle.low <= 0) {
    return false;
  }
  if (candle.high < candle.open || candle.high < candle.close ||
      candle.high < candle.low) {
    return false;
  }
  if (candle.low > candle.open || candle.low > candle.close) {
    return false;
  }
  return true;
}

void DataFetcher::reset() {
  newest_candle_index = -1;
  num_candles = 0;
  last_update_time = 0;
  current_price = 0.0;
  initial_data_loaded = false;
}
