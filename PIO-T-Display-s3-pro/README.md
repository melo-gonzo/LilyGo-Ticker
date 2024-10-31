# Stock Price Tracker for ESP32

A real-time stock price tracker that collects candlestick data using Yahoo Finance API, with configurable intervals and market hours enforcement.

## Overview

This project implements a stock price tracking system on ESP32 that:
- Fetches real-time stock prices from Yahoo Finance
- Creates time-aligned candlestick data
- Respects market trading hours
- Provides test data simulation option
- Stores a configurable number of historical candles

## Features

- Real-time price tracking with configurable intervals
- Time-synchronized candlestick creation
- Market hours enforcement (configurable)
- Weekend detection and handling
- Automatic NTP time synchronization
- Test data generation mode
- Circular buffer for historical candle storage
- Detailed serial output logging

## Configuration

All configuration is managed through the `Config` class in `config.h` and a `credentials.h` file for WiFi. Key settings include:

### Time Settings
```cpp
TimeConfig time = {
    .timezone = "PST8PDT",    // Timezone string
    .ntpServer1 = "pool.ntp.org",
    .ntpServer2 = "time.nist.gov"
};
```

### Stock Tracking Settings
```cpp
StockConfig stock = {
    .symbol = "SPY",          // Stock symbol to track
    .useTestData = false,     // Toggle test data mode
    .updateIntervalMs = 5000, // Price update frequency
    .candleDurationSec = 60,  // Candle duration in seconds
    .maxCandles = 20,         // Maximum candles to store
    .marketHours = {
        .startHour = 6,       // Market open hour (24hr format)
        .startMinute = 30,    // Market open minute
        .endHour = 13,        // Market close hour
        .endMinute = 0,       // Market close minute
        .enforceHours = true  // Toggle market hours enforcement
    }
};
```

## Credentials Setup

For security reasons, WiFi credentials are stored in a separate file that is not tracked by version control.

1. Copy the credentials template:
   ```bash
   cp credentials_template.h credentials.h
   ```

2. Edit `credentials.h` and add your WiFi credentials:
   ```cpp
   namespace StockTracker {
   namespace Credentials {
   
   constexpr const char* WIFI_SSID = "your_actual_ssid";
   constexpr const char* WIFI_PASSWORD = "your_actual_password";
   
   } // namespace Credentials
   } // namespace StockTracker
   ```

3. The `credentials.h` file is included in `.gitignore` to prevent accidentally committing your credentials.

> ⚠️ **Important**: Never commit your `credentials.h` file to version control.


## Important Notes

1. Market Hours:
   - Market hours are in local time (configured timezone)
   - Automatically detects and skips weekends
   - Disabled when using test data
   - Can be toggled with `enforceHours` setting

2. Test Mode:
   - Set `useTestData = true` to use simulated price data
   - Generates random walk prices around a base value
   - Ignores market hours constraints

3. Time Synchronization:
   - Requires working internet connection for NTP sync
   - Candlesticks align to interval boundaries
   - All timestamps are converted to configured timezone

## Setup Instructions

1. Configure WiFi settings in `config.h`
2. Adjust market hours for your timezone
3. Set desired candle duration and update interval
4. Upload to ESP32
5. Monitor via Serial output (115200 baud)

## Serial Output

The system provides detailed status updates via Serial, including:
- Connection status
- Market open/close notifications
- Candlestick data (open, high, low, close)
- Time synchronization status
- Next market open time when market is closed

## Dependencies

- ESP32 Arduino Core
- WiFi library
- HTTPClient library
- ArduinoJson library
- Time library

## Memory Considerations

- Each candle uses approximately 32 bytes
- Default configuration stores 20 candles
- Adjust `maxCandles` based on your client's available memory

## Error Handling

- Failed price fetches are logged and skipped
- Network disconnections are handled gracefully
- Invalid/missing data points are ignored
- Time sync failures prevent operation until resolved

## Contributing

Feel free to submit issues and pull requests for:
- Additional features
- Bug fixes
- Documentation improvements
- Configuration options