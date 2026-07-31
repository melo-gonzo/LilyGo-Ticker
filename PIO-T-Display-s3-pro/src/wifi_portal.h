// wifi_portal.h
//
// WiFi bring-up and management, ported from CandleCollector. Saved networks
// live in NVS; boot scans and joins the strongest known network, and if none
// is reachable raises a fallback access point so the device is always
// reachable for onboarding instead of sitting dark on a "WiFi Connection
// Failed" screen.
//
// HTTP endpoints (registered via wifiPortalRegisterEndpoints):
//   GET /wifi                          management page (scan / join / forget)
//   GET /wifi/status                   station + AP state, saved networks
//   GET /wifi/scan[?start=1]           async scan; poll until {"nets":[...]}
//   GET /wifi/add?ssid=&pass=[&join=1] save (and optionally join) a network
//   GET /wifi/join?i=N                 join saved network N now
//   GET /wifi/del?i=N                  forget saved network N
//
// Fallback AP: SSID/password/mDNS host come from Config.h (AP_SSID/AP_PASS/
// MDNS_HOST), served at http://192.168.8.1 (subnet chosen not to collide
// with the 192.168.4.x LAN). If a legacy credentials.h is present it seeds
// the store on first boot.
//
// Static IP: when USE_STATIC_IP is set the configured address is applied
// before every station join, so the device keeps its known address (the
// ticker is normally reached at 192.168.4.184).

#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>

// Loads saved networks and connects: returns true if joined as a station.
// On failure the fallback AP is up instead, so the caller should start the
// web server regardless of the return value.
bool wifiPortalConnect();

// Registers the /wifi handlers on an already-constructed server. Call before
// server.begin().
void wifiPortalRegisterEndpoints(WebServer &server);

// Call from loop() (~5s cadence): executes joins requested over HTTP,
// reconnects a dropped station link, raises the fallback AP if it stays
// down, and retries saved networks while nobody is using the AP.
void wifiPortalTick();

// True while the fallback AP is broadcasting.
bool wifiPortalIsAp();

// True while joined to a network as a station.
bool wifiPortalIsConnected();

// Re-applies NTP + timezone. Called internally on every station-up event;
// exposed so callers can force a resync.
void wifiPortalSyncTime();

#endif // WIFI_PORTAL_H
