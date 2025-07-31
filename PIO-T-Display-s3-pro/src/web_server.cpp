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
    return R"RAW(<!DOCTYPE html>
<html>
<head>
<title>Stock Tracker Config</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:Arial;max-width:600px;margin:20px auto;padding:20px;background:#f5f5f5}
.container{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{text-align:center;color:#333;margin-bottom:20px}
h2{color:#555;border-bottom:2px solid #007bff;padding-bottom:5px;margin-top:25px}
.form-group{margin-bottom:15px}
.form-row{display:flex;gap:10px}
.form-row .form-group{flex:1}
label{display:block;margin-bottom:5px;font-weight:bold;color:#555}
input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}
input[type="checkbox"]{width:auto;margin-right:8px}
.checkbox-group{display:flex;align-items:center}
button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;margin-top:10px}
button:hover{background:#0056b3}
.status{margin-top:15px;padding:10px;border-radius:4px;display:none}
.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}
.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}
.network-note{background:#e3f2fd;padding:10px;border-radius:4px;margin-top:10px;font-size:14px;color:#1565c0}
.symbol-input{text-transform:uppercase}
.symbol-help{font-size:12px;color:#666;margin-top:5px}
.popular-symbols{margin-top:10px}
.symbol-button{background:#f8f9fa;border:1px solid #dee2e6;color:#495057;padding:4px 8px;margin:2px;border-radius:3px;cursor:pointer;display:inline-block;font-size:12px}
.symbol-button:hover{background:#e9ecef}
.test-data-options{background:#fff3cd;padding:10px;border-radius:4px;margin-top:10px;border:1px solid #ffeaa7}
.fast-update-note{background:#e8f5e8;padding:8px;border-radius:4px;font-size:12px;color:#2e7d2e;margin-top:5px}
</style>
</head>
<body>
<div class="container">
<h1>Stock Tracker Config</h1>
<form id="configForm">

<h2>Stock Configuration</h2>
<div class="form-group">
<label>Stock Symbol:</label>
<input type="text" id="symbol" class="symbol-input" maxlength="8" placeholder="Enter symbol (e.g., AAPL)">
<div class="symbol-help">Enter up to 8 characters. Symbol will be automatically capitalized.</div>
<div class="popular-symbols">
<strong>Popular symbols:</strong><br>
<span class="symbol-button" onclick="setSymbol('SPY')">SPY</span>
<span class="symbol-button" onclick="setSymbol('QQQ')">QQQ</span>
<span class="symbol-button" onclick="setSymbol('NVDA')">NVDA</span>
<span class="symbol-button" onclick="setSymbol('AAPL')">AAPL</span>
<span class="symbol-button" onclick="setSymbol('MSFT')">MSFT</span>
<span class="symbol-button" onclick="setSymbol('GOOGL')">GOOGL</span>
<span class="symbol-button" onclick="setSymbol('AMZN')">AMZN</span>
<span class="symbol-button" onclick="setSymbol('TSLA')">TSLA</span>
<span class="symbol-button" onclick="setSymbol('META')">META</span>
<span class="symbol-button" onclick="setSymbol('BTC-USD')">BTC-USD</span>
<span class="symbol-button" onclick="setSymbol('ETH-USD')">ETH-USD</span>
</div>
</div>
<div class="form-row">
<div class="form-group">
<label>Interval:</label>
<select id="yahooInterval"><option>1m</option><option>2m</option><option>5m</option><option>15m</option><option>30m</option><option>1h</option><option>1d</option></select>
</div>
<div class="form-group">
<label>Range:</label>
<select id="yahooRange"><option>1d</option><option>5d</option><option>1mo</option><option>3mo</option><option>6mo</option><option>1y</option></select>
</div>
</div>

<h2>Display Settings</h2>
<div class="form-row">
<div class="form-group">
<label>Update Interval:</label>
<div style="display:flex;gap:10px;">
<input type="number" id="updateInterval" min="1" value="1" style="flex:1;">
<select id="updateUnit" style="flex:0 0 80px;">
<option value="ms">ms</option>
<option value="sec" selected>sec</option>
</select>
</div>
<div id="updateHelp" style="font-size:12px;color:#666;margin-top:5px;">
How often to fetch new price data from the market
</div>
</div>
<div class="form-group">
<label>Bars to Show:</label>
<input type="number" id="barsToShow" min="1" value="50" style="transition: border-color 0.3s;">
<div id="barsHelpText" style="color:#666;font-size:12px;margin-top:5px;white-space:pre-line;">
Max bars depends on screen resolution and data buffer
</div>
</div>
</div>

<div class="form-group">
<div id="candleDurationInfo" style="background:#e3f2fd;padding:10px;border-radius:4px;font-size:12px;color:#1565c0;">
<strong>Candle Duration:</strong> Auto-synced with interval (<span id="computedDuration">120</span> seconds)
<br><em>Historical and real-time candles will have consistent durations</em>
</div>
</div>

<h2>Data Options</h2>
<div class="form-group">
<div class="checkbox-group">
<input type="checkbox" id="useTestData">
<label>Use Test Data</label>
</div>
<div id="testDataOptions" class="test-data-options" style="display:none;">
<strong>Test Data Mode:</strong> You can use very fast update intervals (like 100ms) to see rapid price changes for testing and demos.
<div class="fast-update-note">
<strong>💡 Tip:</strong> Try 100-500ms intervals for fast test data simulation!
</div>
</div>
</div>
<div class="form-group">
<div style="background:#fff3cd;padding:8px;border-radius:4px;font-size:12px;color:#856404;">
<strong>Real-time Updates:</strong> Always enabled for live price tracking
</div>
</div>
<div class="form-group">
<div class="checkbox-group">
<input type="checkbox" id="enforceHours" checked>
<label>Enforce Market Hours</label>
</div>
</div>

<h2>Network Configuration</h2>
<div class="form-group">
<div class="checkbox-group">
<input type="checkbox" id="useStaticIP">
<label>Use Static IP</label>
</div>
</div>
<div id="staticIPFields" style="display:none;">
<div class="form-group">
<label>Static IP Address:</label>
<input type="text" id="staticIP" placeholder="192.168.4.184">
</div>
<div class="form-row">
<div class="form-group">
<label>Gateway IP:</label>
<input type="text" id="gatewayIP" placeholder="192.168.4.1">
</div>
<div class="form-group">
<label>Subnet Mask:</label>
<input type="text" id="subnetMask" placeholder="255.255.255.0">
</div>
</div>
<div class="network-note">
<strong>Note:</strong> Changing network settings will require a device restart to take effect.
</div>
</div>

<button type="submit">Update Config</button>
</form>
<div id="status" class="status"></div>
</div>
<script>
document.addEventListener('DOMContentLoaded', loadConfig);

// Show/hide test data options
document.getElementById('useTestData').addEventListener('change', function() {
    const testOptions = document.getElementById('testDataOptions');
    testOptions.style.display = this.checked ? 'block' : 'none';
    updateIntervalHelp();
});

// Update help text based on test data mode and unit
function updateIntervalHelp() {
    const isTestMode = document.getElementById('useTestData').checked;
    const unit = document.getElementById('updateUnit').value;
    const helpText = document.getElementById('updateHelp');
    
    if (isTestMode) {
        if (unit === 'ms') {
            helpText.textContent = 'How fast to generate new test data (milliseconds) - try 100-500ms for fast simulation';
            helpText.style.color = '#2e7d2e';
        } else {
            helpText.textContent = 'How fast to generate new test data (seconds)';
            helpText.style.color = '#2e7d2e';
        }
    } else {
        helpText.textContent = 'How often to fetch new price data from the market (minimum 1 second for real data)';
        helpText.style.color = '#666';
        if (unit === 'ms') {
            // Force back to seconds for real data
            document.getElementById('updateUnit').value = 'sec';
        }
    }
}

document.getElementById('updateUnit').addEventListener('change', updateIntervalHelp);

// Auto-capitalize and limit symbol input
document.getElementById('symbol').addEventListener('input', function(e) {
    let value = e.target.value.toUpperCase();
    value = value.replace(/[^A-Z0-9-]/g, '');
    if (value.length > 8) {
        value = value.substring(0, 8);
    }
    e.target.value = value;
});

function setSymbol(symbol) {
    document.getElementById('symbol').value = symbol;
}

document.getElementById('useStaticIP').addEventListener('change', function() {
    const staticFields = document.getElementById('staticIPFields');
    staticFields.style.display = this.checked ? 'block' : 'none';
});

async function loadConfig(){
try{
const response = await fetch('/config');
const config = await response.json();
document.getElementById('symbol').value = config.symbol || 'SPY';
document.getElementById('yahooInterval').value = config.yahooInterval || '1m';
document.getElementById('yahooRange').value = config.yahooRange || '1d';

// Handle update interval with units
let updateMs = config.updateInterval || 1000;
if (updateMs < 1000) {
    document.getElementById('updateInterval').value = updateMs;
    document.getElementById('updateUnit').value = 'ms';
} else {
    document.getElementById('updateInterval').value = Math.round(updateMs / 1000);
    document.getElementById('updateUnit').value = 'sec';
}

document.getElementById('barsToShow').value = config.barsToShow || 50;

const computedDuration = config.computedCandleDuration || 120;
document.getElementById('computedDuration').textContent = computedDuration;

const maxBars = config.maxBars;
document.getElementById('barsToShow').max = maxBars;
let helpText = `Max bars: ${maxBars}`;
document.getElementById('barsHelpText').textContent = helpText;

document.getElementById('useTestData').checked = config.useTestData || false;
document.getElementById('enforceHours').checked = config.enforceHours !== false;

// Show/hide test data options
const testOptions = document.getElementById('testDataOptions');
testOptions.style.display = document.getElementById('useTestData').checked ? 'block' : 'none';

document.getElementById('useStaticIP').checked = config.useStaticIP || false;
document.getElementById('staticIP').value = config.staticIP || '192.168.4.184';
document.getElementById('gatewayIP').value = config.gatewayIP || '192.168.4.1';
document.getElementById('subnetMask').value = config.subnetMask || '255.255.255.0';

const staticFields = document.getElementById('staticIPFields');
staticFields.style.display = document.getElementById('useStaticIP').checked ? 'block' : 'none';

updateIntervalHelp();

}catch(e){console.log('Load config error:',e)}
}

document.getElementById('configForm').addEventListener('submit', async function(e){
e.preventDefault();

// Convert update interval to milliseconds
let updateInterval = parseInt(document.getElementById('updateInterval').value);
const unit = document.getElementById('updateUnit').value;
if (unit === 'sec') {
    updateInterval = updateInterval * 1000; // Convert to milliseconds
}

// Validate minimum intervals
const isTestMode = document.getElementById('useTestData').checked;
if (!isTestMode && updateInterval < 1000) {
    showStatus('Real data mode requires minimum 1 second update interval', 'error');
    return;
}

const config = {
symbol: document.getElementById('symbol').value.trim().toUpperCase(),
yahooInterval: document.getElementById('yahooInterval').value,
yahooRange: document.getElementById('yahooRange').value,
updateInterval: updateInterval, // Now in milliseconds
barsToShow: parseInt(document.getElementById('barsToShow').value),
useTestData: document.getElementById('useTestData').checked,
enforceHours: document.getElementById('enforceHours').checked,
useStaticIP: document.getElementById('useStaticIP').checked,
staticIP: document.getElementById('staticIP').value,
gatewayIP: document.getElementById('gatewayIP').value,
subnetMask: document.getElementById('subnetMask').value
};

try{
const response = await fetch('/config', {
method: 'POST',
headers: {'Content-Type': 'application/json'},
body: JSON.stringify(config)
});
const result = await response.json();
if(response.ok) {
    let message = `Updated successfully! Update interval: ${updateInterval}${unit === 'ms' ? 'ms' : 's'}`;
    setTimeout(loadConfig, 1000);
    showStatus(message, 'success');
} else {
    showStatus('Update failed!', 'error');
}
}catch(e){
showStatus('Error: ' + e.message, 'error');
}
});

function showStatus(message, type){
const status = document.getElementById('status');
status.textContent = message;
status.className = 'status ' + type;
status.style.display = 'block';
setTimeout(() => status.style.display = 'none', 5000);
}
</script>
</body>
</html>)RAW";
}