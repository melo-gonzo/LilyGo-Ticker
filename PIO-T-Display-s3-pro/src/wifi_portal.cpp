// wifi_portal.cpp - see wifi_portal.h

#include "wifi_portal.h"
#include "config.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Optional legacy seed: if src/credentials.h exists, its WIFI_SSID/PASSWORD
// seed the saved-network store on first boot. Not required anymore - the
// portal AP is the primary onboarding path.
#if __has_include("credentials.h")
#include "credentials.h"
#define HAS_SEED_CREDS 1
#endif

#define MAX_NETS 6
#define CONNECT_TIMEOUT_MS 10000
#define BOOT_MAX_ATTEMPTS 4
#define FALLBACK_AP_AFTER_MS 45000 // station down this long -> raise the AP
#define STA_RETRY_PERIOD_MS 60000  // retry saved networks while the AP idles

static char netSsid[MAX_NETS][33];
static char netPass[MAX_NETS][65];
static int netCount = 0;
// Guards the net table: HTTP handlers (add/del) run on the server task while
// the loop task reads it in tryKnown/tryJoin/tick.
static SemaphoreHandle_t netMtx = NULL;

static bool apUp = false;
static volatile int pendingJoin = -1; // saved-net index, executed in tick
static volatile bool pendingDrop = false;
static volatile bool joining = false;
static char joinMsg[96] = "";
static uint32_t staLostMs = 0;
static uint32_t lastStaRetryMs = 0;

static WebServer *wifiServer = NULL;

// ---------------- saved networks (NVS) ----------------

static void saveNets() {
  Preferences p;
  if (!p.begin("wifi", false))
    return;
  p.putInt("n", netCount);
  char key[4];
  for (int i = 0; i < netCount; i++) {
    snprintf(key, sizeof(key), "s%d", i);
    p.putString(key, netSsid[i]);
    snprintf(key, sizeof(key), "p%d", i);
    p.putString(key, netPass[i]);
  }
  p.end();
}

static void loadNets() {
  Preferences p;
  int n = -1;
  if (p.begin("wifi", true)) {
    n = p.getInt("n", -1);
    char key[4];
    for (int i = 0; i < n && i < MAX_NETS; i++) {
      snprintf(key, sizeof(key), "s%d", i);
      p.getString(key, netSsid[i], sizeof(netSsid[i]));
      snprintf(key, sizeof(key), "p%d", i);
      p.getString(key, netPass[i], sizeof(netPass[i]));
    }
    p.end();
  }
  if (n < 0) {
    // First boot: seed from credentials.h if compiled in, else start empty
    // so the device comes up on the AP for onboarding.
    netCount = 0;
#ifdef HAS_SEED_CREDS
    if (StockTracker::Credentials::WIFI_SSID[0] &&
        strcmp(StockTracker::Credentials::WIFI_SSID, "your_ssid_here") != 0) {
      strlcpy(netSsid[0], StockTracker::Credentials::WIFI_SSID,
              sizeof(netSsid[0]));
      strlcpy(netPass[0], StockTracker::Credentials::WIFI_PASSWORD,
              sizeof(netPass[0]));
      netCount = 1;
    }
#endif
    saveNets();
  } else {
    netCount = n > MAX_NETS ? MAX_NETS : n;
  }
}

// Upgrade path from the credentials.h era: firmware before the portal called
// WiFi.begin() with persistence on, so the network it was joined to still
// sits in the SDK's own NVS. Adopt it on first boot instead of stranding a
// working device on the fallback AP. Requires the WiFi driver to be
// initialised (WiFi.mode()) and must run before disconnect(..., eraseap).
static bool seedFromSdkConfig() {
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK)
    return false;
  const char *ssid = (const char *)conf.sta.ssid;
  if (ssid[0] == '\0')
    return false;
  strlcpy(netSsid[0], ssid, sizeof(netSsid[0]));
  strlcpy(netPass[0], (const char *)conf.sta.password, sizeof(netPass[0]));
  netCount = 1;
  Serial.printf("[wifi] adopted \"%s\" from the previous firmware's config\n",
                netSsid[0]);
  return true;
}

// ---------------- connection machinery ----------------

static void startMdns() {
  MDNS.end();
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
  }
}

// Applies the configured static address, or clears any previous one so the
// stack falls back to DHCP. Must run before WiFi.begin() on every attempt.
static void applyIpConfig() {
  if (!USE_STATIC_IP) {
    WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0),
                IPAddress((uint32_t)0));
    return;
  }
  IPAddress ip, gw, mask;
  if (!ip.fromString(STATIC_IP) || !gw.fromString(GATEWAY_IP) ||
      !mask.fromString(SUBNET_MASK)) {
    Serial.println("[wifi] static IP settings are malformed, using DHCP");
    return;
  }
  IPAddress dns1(8, 8, 8, 8), dns2(8, 8, 4, 4);
  if (!WiFi.config(ip, gw, mask, dns1, dns2)) {
    Serial.println("[wifi] static IP configuration failed, using DHCP");
  }
}

void wifiPortalSyncTime() {
  // NTP in UTC. configTime() overwrites the TZ env from its offset args
  // (UTC0 here), so the configured timezone must be re-applied after it -
  // otherwise the device silently runs on UTC and misses market open.
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
  setenv("TZ", TIME_ZONE, 1);
  tzset();
}

static void onStaUp() {
  WiFi.setSleep(false);
  wifiPortalSyncTime();
  startMdns();
  staLostMs = 0;
  Serial.printf("[wifi] connected to \"%s\", IP %s (http://%s.local)\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                MDNS_HOST);
}

static void startAp() {
  if (apUp)
    return;
  // AP_STA (not plain AP) so scans and join attempts still work underneath.
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAPConfig(IPAddress(192, 168, 8, 1), IPAddress(192, 168, 8, 1),
                    IPAddress(255, 255, 255, 0));
  apUp = WiFi.softAP(AP_SSID, AP_PASS);
  if (apUp) {
    startMdns();
    Serial.printf("[wifi] AP \"%s\" up at http://%s (pass: %s)\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str(), AP_PASS);
  } else {
    Serial.println("[wifi] softAP start FAILED");
  }
}

static void stopAp() {
  if (!apUp)
    return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apUp = false;
  Serial.println("[wifi] AP stopped (station link restored)");
}

// Blocking join attempt. Runs in the loop task (boot and tick) only.
static bool tryJoin(int slot) {
  char ssidBuf[33], passBuf[65];
  xSemaphoreTake(netMtx, portMAX_DELAY);
  if (slot < 0 || slot >= netCount) {
    xSemaphoreGive(netMtx);
    return false;
  }
  strlcpy(ssidBuf, netSsid[slot], sizeof(ssidBuf));
  strlcpy(passBuf, netPass[slot], sizeof(passBuf));
  xSemaphoreGive(netMtx);

  joining = true;
  snprintf(joinMsg, sizeof(joinMsg), "joining \"%s\"...", ssidBuf);
  Serial.printf("[wifi] trying \"%s\"\n", ssidBuf);
  WiFi.disconnect();
  applyIpConfig();
  WiFi.begin(ssidBuf, passBuf);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT_MS) {
    delay(100);
  }
  joining = false;
  if (WiFi.status() == WL_CONNECTED) {
    onStaUp();
    snprintf(joinMsg, sizeof(joinMsg), "connected to \"%s\" - %s", ssidBuf,
             WiFi.localIP().toString().c_str());
    return true;
  }
  snprintf(joinMsg, sizeof(joinMsg),
           "could not join \"%s\" (wrong password or out of range)", ssidBuf);
  Serial.printf("[wifi] \"%s\" failed\n", ssidBuf);
  return false;
}

// Scan and try saved networks, strongest first; unseen ones (hidden SSIDs)
// are tried last.
static bool tryKnown() {
  char snapSsid[MAX_NETS][33];
  int snapCount;
  xSemaphoreTake(netMtx, portMAX_DELAY);
  snapCount = netCount;
  for (int i = 0; i < snapCount; i++)
    strlcpy(snapSsid[i], netSsid[i], sizeof(snapSsid[i]));
  xSemaphoreGive(netMtx);
  if (snapCount == 0)
    return false;

  int16_t found = WiFi.scanNetworks(); // blocking, ~2-3s

  int order[MAX_NETS], rssi[MAX_NETS], cand = 0;
  for (int i = 0; i < snapCount; i++) {
    for (int j = 0; j < found; j++) {
      if (WiFi.SSID(j) == snapSsid[i]) {
        order[cand] = i;
        rssi[cand++] = WiFi.RSSI(j);
        break;
      }
    }
  }
  for (int a = 0; a < cand; a++) // strongest first
    for (int b = a + 1; b < cand; b++)
      if (rssi[b] > rssi[a]) {
        int t = order[a];
        order[a] = order[b];
        order[b] = t;
        t = rssi[a];
        rssi[a] = rssi[b];
        rssi[b] = t;
      }
  for (int i = 0; i < snapCount && cand < MAX_NETS; i++) { // hidden SSIDs
    bool seen = false;
    for (int a = 0; a < cand; a++)
      seen |= (order[a] == i);
    if (!seen)
      order[cand++] = i;
  }
  WiFi.scanDelete();

  int tries = cand < BOOT_MAX_ATTEMPTS ? cand : BOOT_MAX_ATTEMPTS;
  for (int a = 0; a < tries; a++) {
    if (tryJoin(order[a]))
      return true;
  }
  return false;
}

// ---------------- public API ----------------

bool wifiPortalIsAp() { return apUp; }

bool wifiPortalIsConnected() { return WiFi.status() == WL_CONNECTED; }

bool wifiPortalConnect() {
  if (!netMtx)
    netMtx = xSemaphoreCreateMutex();
  // Bring the driver up first so the SDK's stored station config is readable
  // for the one-time migration below.
  WiFi.mode(WIFI_STA);
  loadNets();
  if (netCount == 0 && seedFromSdkConfig()) {
    saveNets();
  }
  WiFi.persistent(false); // creds live in our own NVS namespace, not the SDK's
  WiFi.disconnect(false, true); // clear the SDK's cached STA credentials
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (tryKnown())
    return true;
  Serial.println("[wifi] no saved network reachable - raising fallback AP");
  startAp();
  lastStaRetryMs = millis();
  return false;
}

void wifiPortalTick() {
  // Forgot the network we were on (over HTTP): drop the live link.
  if (pendingDrop) {
    pendingDrop = false;
    WiFi.disconnect(false, true);
    staLostMs = 0;
    xSemaphoreTake(netMtx, portMAX_DELAY);
    int nLeft = netCount;
    xSemaphoreGive(netMtx);
    if (nLeft == 0 || !tryKnown())
      startAp();
    else if (apUp && WiFi.softAPgetStationNum() == 0)
      stopAp();
    return;
  }

  // Join requested over HTTP.
  if (pendingJoin >= 0) {
    int slot = pendingJoin;
    pendingJoin = -1;
    if (!tryJoin(slot)) {
      if (!tryKnown())
        startAp();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    staLostMs = 0;
    // drop a leftover fallback AP once nobody is using it
    if (apUp && WiFi.softAPgetStationNum() == 0)
      stopAp();
    return;
  }

  uint32_t now = millis();
  if (staLostMs == 0) {
    staLostMs = now;
    Serial.println("[wifi] station link lost, reconnecting...");
    WiFi.reconnect();
    return;
  }
  if (!apUp && now - staLostMs > FALLBACK_AP_AFTER_MS) {
    startAp();
    lastStaRetryMs = now;
    return;
  }
  // Periodic full retry - but never while someone is browsing via the AP,
  // because join attempts hop channels and would kick them off.
  if (now - lastStaRetryMs > STA_RETRY_PERIOD_MS &&
      (!apUp || WiFi.softAPgetStationNum() == 0)) {
    lastStaRetryMs = now;
    if (tryKnown() && apUp && WiFi.softAPgetStationNum() == 0)
      stopAp();
  }
}

// ---------------- HTTP handlers ----------------

static String jsonEsc(const String &s) {
  String o;
  o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      o += '\\';
      o += c;
    } else if ((uint8_t)c >= 0x20)
      o += c;
  }
  return o;
}

static void handleWifiStatus() {
  bool sta = WiFi.status() == WL_CONNECTED;
  String j;
  j.reserve(512);
  j += "{\"joining\":";
  j += (joining || pendingJoin >= 0) ? "true" : "false";
  j += ",\"msg\":\"";
  j += jsonEsc(joinMsg);
  j += '"';
  j += ",\"sta\":{\"up\":";
  j += sta ? "true" : "false";
  if (sta) {
    j += ",\"ssid\":\"";
    j += jsonEsc(WiFi.SSID());
    j += "\",\"ip\":\"";
    j += WiFi.localIP().toString();
    j += "\",\"rssi\":";
    j += String(WiFi.RSSI());
  }
  j += "},\"ap\":{\"up\":";
  j += apUp ? "true" : "false";
  if (apUp) {
    j += ",\"ip\":\"";
    j += WiFi.softAPIP().toString();
    j += "\",\"stations\":";
    j += String(WiFi.softAPgetStationNum());
  }
  j += "},\"nets\":[";
  for (int i = 0; i < netCount; i++) {
    if (i)
      j += ',';
    j += "{\"i\":";
    j += String(i);
    j += ",\"ssid\":\"";
    j += jsonEsc(netSsid[i]);
    j += "\",\"cur\":";
    j += (sta && WiFi.SSID() == netSsid[i]) ? "true" : "false";
    j += '}';
  }
  j += "]}";
  wifiServer->send(200, "application/json", j);
}

static void handleWifiScan() {
  if (wifiServer->hasArg("start")) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true); // async; poll this endpoint until done
    wifiServer->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    wifiServer->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (n < 0)
    n = 0;
  String j;
  j.reserve(1024);
  j += "{\"scanning\":false,\"nets\":[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0)
      continue;
    bool dup = false; // keep only the strongest BSSID per SSID
    for (int k = 0; k < i; k++)
      dup |= (WiFi.SSID(k) == s);
    if (dup)
      continue;
    bool known = false;
    for (int k = 0; k < netCount; k++)
      known |= (s == netSsid[k]);
    if (!first)
      j += ',';
    first = false;
    j += "{\"ssid\":\"";
    j += jsonEsc(s);
    j += "\",\"rssi\":";
    j += String(WiFi.RSSI(i));
    j += ",\"sec\":";
    j += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
    j += ",\"known\":";
    j += known ? "true" : "false";
    j += '}';
  }
  j += "]}";
  wifiServer->send(200, "application/json", j);
}

static void handleWifiAdd() {
  String s = wifiServer->arg("ssid");
  String pw = wifiServer->arg("pass");
  if (s.length() == 0 || s.length() > 32 || pw.length() > 64) {
    wifiServer->send(400, "text/plain", "ssid must be 1..32 chars, pass <= 64");
    return;
  }
  xSemaphoreTake(netMtx, portMAX_DELAY);
  int slot = -1;
  for (int i = 0; i < netCount; i++)
    if (s == netSsid[i])
      slot = i; // re-adding updates the password
  if (slot < 0) {
    if (netCount >= MAX_NETS) {
      xSemaphoreGive(netMtx);
      wifiServer->send(507, "text/plain",
                       "all " + String(MAX_NETS) +
                           " slots used - forget one first");
      return;
    }
    slot = netCount++;
  }
  strlcpy(netSsid[slot], s.c_str(), sizeof(netSsid[slot]));
  strlcpy(netPass[slot], pw.c_str(), sizeof(netPass[slot]));
  xSemaphoreGive(netMtx);
  saveNets();
  if (wifiServer->arg("join") == "1") {
    pendingJoin = slot;
    snprintf(joinMsg, sizeof(joinMsg), "joining \"%s\"...", netSsid[slot]);
  }
  wifiServer->send(200, "application/json", "{\"saved\":" + String(slot) + "}");
}

static void handleWifiJoin() {
  int i = wifiServer->hasArg("i") ? atoi(wifiServer->arg("i").c_str()) : -1;
  if (i < 0 || i >= netCount) {
    wifiServer->send(400, "text/plain", "bad index");
    return;
  }
  pendingJoin = i;
  snprintf(joinMsg, sizeof(joinMsg), "joining \"%s\"...", netSsid[i]);
  wifiServer->send(200, "application/json", "{\"joining\":true}");
}

static void handleWifiDel() {
  int i = wifiServer->hasArg("i") ? atoi(wifiServer->arg("i").c_str()) : -1;
  if (i < 0 || i >= netCount) {
    wifiServer->send(400, "text/plain", "bad index");
    return;
  }
  bool wasActive = WiFi.status() == WL_CONNECTED && WiFi.SSID() == netSsid[i];
  xSemaphoreTake(netMtx, portMAX_DELAY);
  for (int k = i; k < netCount - 1; k++) {
    strlcpy(netSsid[k], netSsid[k + 1], sizeof(netSsid[k]));
    strlcpy(netPass[k], netPass[k + 1], sizeof(netPass[k]));
  }
  netCount--;
  xSemaphoreGive(netMtx);
  saveNets();
  if (wasActive)
    pendingDrop = true;
  wifiServer->send(200, "application/json", "{\"ok\":true}");
}

// ---------------- page ----------------

static const char WIFI_PAGE[] PROGMEM = R"html(<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>LilyGo Ticker WiFi</title><style>
body{font-family:system-ui,sans-serif;max-width:480px;margin:1rem auto;padding:0 1rem;background:#111;color:#eee}
h2{margin:.5rem 0}button{padding:.4rem .8rem;margin:.2rem;border:0;border-radius:6px;background:#2a6;color:#fff;cursor:pointer}
button.del{background:#a33}input{padding:.4rem;margin:.2rem 0;width:100%;box-sizing:border-box;border-radius:6px;border:1px solid #555;background:#222;color:#eee}
li{margin:.3rem 0;list-style:none}ul{padding:0}.net{cursor:pointer;text-decoration:underline}
a{color:#6cf}#msg{color:#fc6;min-height:1.2em}</style></head><body>
<h2>LilyGo Ticker WiFi</h2>
<div id="msg"></div><div id="sta"></div>
<h3>Saved networks</h3><ul id="saved"></ul>
<h3>Add a network</h3>
<input id="ssid" placeholder="SSID"><input id="pass" placeholder="password" type="password">
<button onclick="add()">Save &amp; join</button>
<h3>Nearby <button onclick="scan()">Scan</button></h3><ul id="scan"></ul>
<p><a href="/">&larr; ticker config</a> &middot; <a href="/status">status</a></p>
<script>
function j(u,cb){fetch(u).then(r=>r.json()).then(cb).catch(()=>{})}
function refresh(){j('/wifi/status',d=>{
 document.getElementById('msg').textContent=d.msg||'';
 document.getElementById('sta').textContent=d.sta.up?('Connected: '+d.sta.ssid+' ('+d.sta.ip+', '+d.sta.rssi+' dBm)'):(d.joining?'Joining...':'Not connected');
 document.getElementById('saved').innerHTML=d.nets.map(n=>'<li>'+n.ssid+(n.cur?' &#10003;':'')+' <button onclick="join('+n.i+')">Join</button><button class="del" onclick="del('+n.i+')">Forget</button></li>').join('')||'<li>(none)</li>';})}
function scan(){j('/wifi/scan?start=1',()=>{poll()})}
function poll(){j('/wifi/scan',d=>{if(d.scanning){setTimeout(poll,1200);return}
 document.getElementById('scan').innerHTML=d.nets.map(n=>'<li class="net" onclick="pick(\''+n.ssid.replace(/'/g,"\\'")+'\')">'+n.ssid+' ('+n.rssi+' dBm)'+(n.sec?' &#128274;':'')+(n.known?' [saved]':'')+'</li>').join('')||'<li>(nothing found)</li>'})}
function pick(s){document.getElementById('ssid').value=s;document.getElementById('pass').focus()}
function add(){var s=encodeURIComponent(document.getElementById('ssid').value),p=encodeURIComponent(document.getElementById('pass').value);
 fetch('/wifi/add?ssid='+s+'&pass='+p+'&join=1').then(()=>{document.getElementById('pass').value=''})}
function join(i){fetch('/wifi/join?i='+i)}
function del(i){fetch('/wifi/del?i='+i)}
setInterval(refresh,3000);refresh();
</script></body></html>)html";

static void handleWifiPage() { wifiServer->send_P(200, "text/html", WIFI_PAGE); }

void wifiPortalRegisterEndpoints(WebServer &server) {
  wifiServer = &server;
  server.on("/wifi", HTTP_GET, handleWifiPage);
  server.on("/wifi/status", HTTP_GET, handleWifiStatus);
  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/add", HTTP_GET, handleWifiAdd);
  server.on("/wifi/join", HTTP_GET, handleWifiJoin);
  server.on("/wifi/del", HTTP_GET, handleWifiDel);
}
