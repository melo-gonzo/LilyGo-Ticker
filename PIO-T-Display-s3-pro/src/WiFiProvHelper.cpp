#include "WiFiProvHelper.h"
#include <WiFi.h>
#include <WiFiProv.h>

void SysProvEvent(arduino_event_t *sys_event) {
    switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("[DEBUG] Wi-Fi connected successfully.");
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("[DEBUG] Disconnected from Wi-Fi.");
        break;
    case ARDUINO_EVENT_PROV_START:
        Serial.println("[DEBUG] Provisioning started. Please connect to Wi-Fi 'PROV_XXXXXX' to set up device.");
        break;
    case ARDUINO_EVENT_PROV_END:
        Serial.println("[DEBUG] Provisioning process completed.");
        break;
    default:
        break;
    }
}

bool isProvisioned() {
    return WiFi.isConnected();
}

void setupProvisioning(const char *pop, const char *service_name, const char *service_key, bool reset_provisioned) {
    WiFi.onEvent(SysProvEvent);
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
}

void resetProvisioning() {
    WiFi.disconnect(true);
    ESP.restart();
}