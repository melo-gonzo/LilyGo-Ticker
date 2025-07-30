### LilyGo Ticker
Inspired from [this project](https://github.com/nishad2m8/BTC-Ticker), I wanted a simple way to keep up with market price movements during the intraday time frame. This project heavily refactors the linked projects code to accomplish a few key tasks:
1. Pull intraday stock market data for any ticker from Yahoo Finance
2. Display real-time candlestick charts with configurable intervals and bar counts
3. Provide a web interface for dynamic configuration
4. Support 1-pixel wide candles for maximum data density

The specific board I have been using is the [T-Display S3 AMOLED](https://lilygo.cc/products/t-display-s3-amoled-us?_pos=4&_sid=1d7fdbdc0&_ss=r)
If you need a sweet case, check out [this one](https://www.printables.com/model/1222074-lilygo-t-display-s3-amoled-version-20-case-remix) on Printables! 
![image](IMG_0026.JPG)

### Getting Started
1. **Clone the repo**
   ```
   git clone https://github.com/melo-gonzo/LilyGo-Ticker.git
   ```

2. **Setup credentials**
   - Copy `src/credentials_template.h` to `src/credentials.h`
   - Add your WiFi SSID and password

3. **Flash and configure**
   - Build/upload using PlatformIO
   - Connect to the web interface at the device's IP address
   - Configure stock symbol, interval, and display settings

### Web Configuration
The device creates a web interface for easy configuration:
- **Stock Symbol**: Enter any ticker (AAPL, NVDA, BTC-USD, etc.)
- **Interval**: 1m, 2m, 5m, 15m, 30m, 1h, 1d intervals
- **Bars to Show**: Configurable based on screen resolution
- **Network Settings**: Static IP or DHCP

Key features:
- Auto-synced candle duration with selected interval
- Real-time price updates always enabled
- Y-axis scaling based on visible data only
- Market hours enforcement (optional)

### Display Features
- **Candlestick Charts**: Green/red candles with proper OHLC visualization
- **Real-time Updates**: Live price line and incomplete candle highlighting
- **Smart Scaling**: 1-pixel minimum candle width for maximum data density
- **Market Status**: Visual indicator when market is closed
- **Price Range**: Dynamic Y-axis scaling to visible bars only

### Development and Contribution
I took this project as an opportunity to test out some of the latest and greatest LLM's for development. I'm a c++ novice, and thus this was a great opportunity to learn. I stuck primarily with the Claude family of models. I found that the "projects" feature was not super helpful, and that pasting the full codebase (or relevant parts) into the context was most helpful for getting assistance. Therefore, I've included the `print_contents.py` script which is helpful for collating the project into one file that can be copy-pasted into the prompt.

Feel free to contribute via Issues and PR's, I will happily review and incorporate changes where necessary.

### Future Plans
- Interval/range combination validation based on Yahoo Finance API limits
- Data persistence across reboots
- Additional technical indicators overlay
- Multiple ticker support with swipe navigation
- Integration with external databases for historical data