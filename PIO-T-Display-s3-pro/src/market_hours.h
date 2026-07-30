#pragma once

#include "config.h" // For USE_TEST_DATA / ENFORCE_MARKET_HOURS
#include <string>
#include <time.h>

namespace StockTracker {

// Regular US equity session expressed in the device timezone (TIME_ZONE,
// US Mountain): 9:30-16:00 ET is 7:30-14:00 MT. Both zones observe DST on
// the same dates, so the offset between them never moves.
//
// TIME_ZONE carries full POSIX DST rules, so these stay correct across the
// spring/fall transitions - with a bare "MST7MDT" newlib applies standard
// time year-round and every one of these comparisons lands an hour off.
const int MARKET_OPEN_HOUR = 7; // 7:30 AM MT
const int MARKET_OPEN_MINUTE = 30;
const int MARKET_CLOSE_HOUR = 14; // 2:00 PM MT
const int MARKET_CLOSE_MINUTE = 0;

class MarketHoursChecker {
public:
  // Any epoch past this means NTP has actually run (Nov 2023). The old
  // `now < 8 * 3600 * 2` check passes for any time after Jan 1 1970 08:00
  // UTC, so a device that never synced looked like it had a valid clock.
  static bool clockIsSynced() { return time(nullptr) > 1700000000; }

  static bool isMarketOpen() { return withinWindow(0); }

  // Like isMarketOpen(), but stays true for graceMinutes past the close so
  // the final candle of the session - which only completes after the bell -
  // is still fetched.
  static bool isCollectionWindow(int graceMinutes) {
    return withinWindow(graceMinutes);
  }

  static std::string getNextMarketOpen() {
    if (!clockIsSynced()) {
      return "Time not synced";
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int openMinutes = MARKET_OPEN_HOUR * 60 + MARKET_OPEN_MINUTE;

    timeinfo.tm_hour = MARKET_OPEN_HOUR;
    timeinfo.tm_min = MARKET_OPEN_MINUTE;
    timeinfo.tm_sec = 0;

    // Past today's open: the next one is tomorrow.
    if (currentMinutes >= openMinutes) {
      timeinfo.tm_mday += 1;
    }
    mktime(&timeinfo); // normalise, and refresh tm_wday

    while (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) {
      timeinfo.tm_mday += 1;
      mktime(&timeinfo);
    }

    char buffer[40];
    strftime(buffer, sizeof(buffer), "%a %b %d %H:%M %Z", &timeinfo);
    return std::string(buffer);
  }

private:
  static bool withinWindow(int graceMinutes) {
    if (USE_TEST_DATA || !ENFORCE_MARKET_HOURS) {
      return true;
    }
    // Without a real clock we cannot tell open from closed. Assume open so a
    // boot before NTP settles still populates the chart.
    if (!clockIsSynced()) {
      return true;
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) {
      return false;
    }

    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int startMinutes = MARKET_OPEN_HOUR * 60 + MARKET_OPEN_MINUTE;
    int endMinutes =
        MARKET_CLOSE_HOUR * 60 + MARKET_CLOSE_MINUTE + graceMinutes;

    return currentMinutes >= startMinutes && currentMinutes < endMinutes;
  }
};

} // namespace StockTracker
