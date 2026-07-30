// ota_update.h
//
// Over-the-air firmware updates via ArduinoOTA (ported from CandleCollector),
// so the ticker can be re-flashed without unmounting it from its case:
//
//   pio run -e T-Display-AMOLED-ota -t upload
//
// The board's default_16MB.csv partition table already has dual app slots
// (app0/app1), so no partition change is required - one USB flash of an
// OTA-enabled build and every update after that is wireless.
//
// Auth: the OTA password is AP_PASS (the fallback-AP password); the -ota
// env's --auth flag must match. LAN-only threat model, like the AP.
// mDNS: ArduinoOTA's own mDNS init is disabled so it can't clobber the http
// service record the WiFi portal registers; espota connects directly to
// port 3232 and needs no service advertisement.
//
// A transfer can stall if a Yahoo TLS fetch is in flight. A failed OTA is
// harmless - the inactive slot is discarded - so just retry.

#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

// Compile-time build id, surfaced in /status as "fw" so you can confirm an
// update actually landed.
#ifndef FW_VERSION
#define FW_VERSION __DATE__ " " __TIME__
#endif

// Set up ArduinoOTA (hostname, password, callbacks) and start its listener.
// Call once after the WiFi portal is up (works on station or fallback AP).
void otaInit();

// Pump ArduinoOTA. Call every loop(); blocks internally during a transfer
// and reboots the device on success.
void otaHandle();

// True while a transfer is in progress, so the main loop can stand down
// instead of firing an HTTPS fetch that would stall the upload.
bool otaInProgress();

#endif // OTA_UPDATE_H
