#include "web_server.h"
#include "config.h"
#include <WiFi.h>

// Static member definitions
WebServer StockWebServer::server(80);
bool StockWebServer::serverStarted = false;

bool StockWebServer::begin(int port) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot start web server");
        return false;
    }
    
    // Set up routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_GET, handleGetConfig);
    server.on("/config", HTTP_POST, handleSetConfig);
    server.onNotFound(handleNotFound);
    
    // Enable CORS
    server.enableCORS(true);
    
    server.begin();
    serverStarted = true;
    
    Serial.println("Web server started on http://" + WiFi.localIP().toString());
    return true;
}

void StockWebServer::handleClient() {
    if (serverStarted) {
        server.handleClient();
    }
}

void StockWebServer::stop() {
    if (serverStarted) {
        server.stop();
        serverStarted = false;
        Serial.println("Web server stopped");
    }
}

void StockWebServer::handleRoot() {
    server.send(200, "text/html", generateHTML());
}

void StockWebServer::handleGetConfig() {
    String config = getConfigJSON();
    server.send(200, "application/json", config);
}

void StockWebServer::handleSetConfig() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        Serial.println("Received config: " + body);
        
        if (setConfigFromJSON(body)) {
            server.send(200, "application/json", "{\"status\":\"success\"}");
            Serial.println("Configuration updated successfully");
        } else {
            server.send(400, "application/json", "{\"status\":\"error\"}");
        }
    } else {
        server.send(400, "application/json", "{\"status\":\"error\"}");
    }
}

void StockWebServer::handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

String StockWebServer::generateHTML() {
    return R"(<!DOCTYPE html>
<html>
<head>
<title>Stock Tracker Config</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:Arial;max-width:500px;margin:20px auto;padding:20px;background:#f5f5f5}
.container{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{text-align:center;color:#333;margin-bottom:20px}
.form-group{margin-bottom:15px}
label{display:block;margin-bottom:5px;font-weight:bold;color:#555}
input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}
input[type="checkbox"]{width:auto;margin-right:8px}
.checkbox-group{display:flex;align-items:center}
button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;margin-top:10px}
button:hover{background:#0056b3}
.status{margin-top:15px;padding:10px;border-radius:4px;display:none}
.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}
.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}
</style>
</head>
<body>
<div class="container">
<h1>Stock Tracker Config</h1>
<form id="configForm">
<div class="form-group">
<label>Symbol:</label>
<select id="symbol"><option>SPY</option><option>QQQ</option><option>NVDA</option><option>AAPL</option><option>MSFT</option></select>
</div>
<div class="form-group">
<label>Interval:</label>
<select id="yahooInterval"><option>1m</option><option>2m</option><option>5m</option><option>15m</option><option>30m</option><option>1h</option><option>1d</option></select>
</div>
<div class="form-group">
<label>Range:</label>
<select id="yahooRange"><option>1d</option><option>5d</option><option>1mo</option><option>3mo</option><option>6mo</option><option>1y</option></select>
</div>
<div class="form-group">
<label>Update Interval (sec):</label>
<input type="number" id="updateInterval" min="1" max="300" value="1">
</div>
<div class="form-group">
<label>Candle Duration (sec):</label>
<input type="number" id="candleDuration" min="1" max="86400" value="180">
</div>
<div class="form-group">
<label>Bars to Show:</label>
<input type="number" id="barsToShow" min="1" max="120" value="50">
<small style="color:#666;font-size:12px;">Max bars depends on screen resolution</small>
</div>
<div class="form-group">
<div class="checkbox-group">
<input type="checkbox" id="useTestData">
<label>Use Test Data</label>
</div>
</div>
<div class="form-group">
<div class="checkbox-group">
<input type="checkbox" id="useIntraday" checked>
<label>Use Intraday Data</label>
</div>
</div>
<div class="form-group">
<div class="checkbox-group">
<input type="checkbox" id="enforceHours" checked>
<label>Enforce Market Hours</label>
</div>
</div>
<button type="submit">Update Config</button>
</form>
<div id="status" class="status"></div>
</div>
<script>
document.addEventListener('DOMContentLoaded', loadConfig);
async function loadConfig(){
try{
const response = await fetch('/config');
const config = await response.json();
document.getElementById('symbol').value = config.symbol || 'SPY';
document.getElementById('yahooInterval').value = config.yahooInterval || '1m';
document.getElementById('yahooRange').value = config.yahooRange || '1d';
document.getElementById('updateInterval').value = config.updateInterval || 1;
document.getElementById('candleDuration').value = config.candleDuration || 180;
document.getElementById('barsToShow').value = config.barsToShow || 50;
document.getElementById('barsToShow').max = config.maxBars || 120;
document.getElementById('useTestData').checked = config.useTestData || false;
document.getElementById('useIntraday').checked = config.useIntraday !== false;
document.getElementById('enforceHours').checked = config.enforceHours !== false;
}catch(e){console.log('Load config error:',e)}
}
document.getElementById('configForm').addEventListener('submit', async function(e){
e.preventDefault();
const config = {
symbol: document.getElementById('symbol').value,
yahooInterval: document.getElementById('yahooInterval').value,
yahooRange: document.getElementById('yahooRange').value,
updateInterval: parseInt(document.getElementById('updateInterval').value),
candleDuration: parseInt(document.getElementById('candleDuration').value),
barsToShow: parseInt(document.getElementById('barsToShow').value),
useTestData: document.getElementById('useTestData').checked,
useIntraday: document.getElementById('useIntraday').checked,
enforceHours: document.getElementById('enforceHours').checked
};
try{
const response = await fetch('/config', {
method: 'POST',
headers: {'Content-Type': 'application/json'},
body: JSON.stringify(config)
});
const result = await response.json();
showStatus(response.ok ? 'Updated successfully!' : 'Update failed!', response.ok ? 'success' : 'error');
}catch(e){
showStatus('Error: ' + e.message, 'error');
}
});
function showStatus(message, type){
const status = document.getElementById('status');
status.textContent = message;
status.className = 'status ' + type;
status.style.display = 'block';
setTimeout(() => status.style.display = 'none', 3000);
}
</script>
</body>
</html>)";
}