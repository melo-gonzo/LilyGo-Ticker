// ota_update.cpp - see ota_update.h for the design overview.

#include "ota_update.h"
#include "config.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

static bool otaActive = false;

void otaInit() {
  ArduinoOTA.setHostname(MDNS_HOST);
  ArduinoOTA.setPassword(AP_PASS);
  // wifi_portal owns the shared mDNS instance; a second MDNS.begin() from
  // ArduinoOTA would drop the http service record.
  ArduinoOTA.setMdnsEnabled(false);

  ArduinoOTA.onStart([]() {
    otaActive = true;
    Serial.println("[ota] update starting");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static unsigned int lastPct = 101;
    unsigned int pct = total ? (done * 100 / total) : 0;
    if (pct != lastPct && pct % 10 == 0) {
      Serial.printf("[ota] %u%%\n", pct);
      lastPct = pct;
    }
  });
  ArduinoOTA.onEnd([]() {
    otaActive = false;
    Serial.println("[ota] update complete - rebooting");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    otaActive = false;
    Serial.printf("[ota] error %u - resuming normal operation\n",
                  (unsigned)err);
  });
  ArduinoOTA.begin();
  Serial.printf("[ota] listening on port 3232 (fw: %s)\n", FW_VERSION);
}

void otaHandle() { ArduinoOTA.handle(); }

bool otaInProgress() { return otaActive; }
