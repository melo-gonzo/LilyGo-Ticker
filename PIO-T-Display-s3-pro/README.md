# Need to update: 

// Update the UI with the latest data
updateStockUI(btcRate, highRate, lowRate);


for (int i = 0; i < MAX_CANDLES; i++) {
    JsonArray candle = doc[i].as<JsonArray>();
    timestamps[i] = candle[0].as<long>();  // Store the timestamp
    candles[i].open = candle[1].as<float>();
    candles[i].close = candle[4].as<float>();
    candles[i].high = candle[2].as<float>();
    candles[i].low = candle[3].as<float>();

    // Serial.printf("Candle %d - Open: %.2f, Close: %.2f, High: %.2f, Low: %.2f\n", i, candles[i].open, candles[i].close, candles[i].high, candles[i].low);
}