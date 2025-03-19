#pragma once

#include <time.h>
#include <string>
#include "config.h"  // For USE_TEST_DATA

namespace StockTracker {

// Market hours definitions (PST)
const int MARKET_OPEN_HOUR = 6;    // 6:30 AM PST
const int MARKET_OPEN_MINUTE = 30;
const int MARKET_CLOSE_HOUR = 13;  // 1:00 PM PST
const int MARKET_CLOSE_MINUTE = 0;

class MarketHoursChecker {
public:
    static bool isMarketOpen() {
        // If using test data or market hours enforcement is disabled, always return true
        if (USE_TEST_DATA || !ENFORCE_MARKET_HOURS) {
            return true;
        }

        // Check if we have a valid time before making determinations
        time_t now;
        time(&now);
        if (now < 8 * 3600 * 2) { // Basic check if time is set properly
            return true; // Default to open if time isn't properly set
        }
        
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        // Check if it's weekend
        if (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) {
            return false;
        }

        // Convert current time to minutes since midnight
        int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

        // Convert market hours to minutes since midnight
        int marketStartMinutes = MARKET_OPEN_HOUR * 60 + MARKET_OPEN_MINUTE;
        int marketEndMinutes = MARKET_CLOSE_HOUR * 60 + MARKET_CLOSE_MINUTE;

        return currentMinutes >= marketStartMinutes && currentMinutes < marketEndMinutes;
    }

    static std::string getNextMarketOpen() {
        time_t now;
        time(&now);
        
        // Basic check if time is set properly
        if (now < 8 * 3600 * 2) {
            return "Time not synced";
        }
        
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        // Set time to next market open
        timeinfo.tm_hour = MARKET_OPEN_HOUR;
        timeinfo.tm_min = MARKET_OPEN_MINUTE;
        timeinfo.tm_sec = 0;

        // If we're past today's market open time but before market close, 
        // keep the same day, just show today's opening time
        if (!isMarketOpen()) {
            int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            int marketEndMinutes = MARKET_CLOSE_HOUR * 60 + MARKET_CLOSE_MINUTE;
            
            if (currentMinutes >= marketEndMinutes) {
                // Already past market close, move to next day
                timeinfo.tm_mday += 1;
                mktime(&timeinfo); // Normalize date
            }
        }

        // Skip weekends
        while (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) {
            timeinfo.tm_mday += 1;
            mktime(&timeinfo);  // Normalize date and get proper day of week
        }

        char buffer[30];
        strftime(buffer, sizeof(buffer), "%a %b %d %H:%M %Z", &timeinfo);
        return std::string(buffer);
    }
};

} // namespace StockTracker