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
.form-group input[type="number"]:invalid,
.form-group input[type="number"].invalid {
    border-color: #dc3545 !important;
    box-shadow: 0 0 0 0.2rem rgba(220, 53, 69, 0.25);
}
.form-group input[type="number"]:valid,
.form-group input[type="number"].valid {
    border-color: #28a745 !important;
}
.bars-limitation-info {
    background: #e3f2fd;
    border: 1px solid #90caf9;
    border-radius: 4px;
    padding: 10px;
    margin-top: 10px;
    font-size: 12px;
    color: #1565c0;
}
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
<label>Update Interval (sec):</label>
<input type="number" id="updateInterval" min="1" max="300" value="1">
<div style="font-size:12px;color:#666;margin-top:5px;">
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

// Auto-capitalize and limit symbol input
document.getElementById('symbol').addEventListener('input', function(e) {
    let value = e.target.value.toUpperCase();
    // Remove any non-alphanumeric characters except hyphens (for crypto pairs like BTC-USD)
    value = value.replace(/[^A-Z0-9-]/g, '');
    // Limit to 8 characters
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

// Add input validation for bars field
document.getElementById('barsToShow').addEventListener('input', function(e) {
    const value = parseInt(e.target.value);
    const max = parseInt(e.target.max);
    const min = parseInt(e.target.min) || 1;
    
    if (value > max) {
        e.target.style.borderColor = '#dc3545';
        e.target.title = `Maximum allowed: ${max}`;
    } else if (value < min) {
        e.target.style.borderColor = '#dc3545';
        e.target.title = `Minimum allowed: ${min}`;
    } else {
        e.target.style.borderColor = '#28a745';
        e.target.title = '';
    }
});

async function loadConfig(){
try{
const response = await fetch('/config');
const config = await response.json();
document.getElementById('symbol').value = config.symbol || 'SPY';
document.getElementById('yahooInterval').value = config.yahooInterval || '1m';
document.getElementById('yahooRange').value = config.yahooRange || '1d';
document.getElementById('updateInterval').value = config.updateInterval || 1;
// Remove: document.getElementById('candleDuration').value = config.candleDuration || 180;
document.getElementById('barsToShow').value = config.barsToShow || 50;

// Show computed candle duration
const computedDuration = config.computedCandleDuration || 120;
document.getElementById('computedDuration').textContent = computedDuration;

// Update bars validation with more detailed info
const maxBars = config.maxBars;
const maxBarsScreen = config.maxBarsScreen || maxBars;
const maxBarsData = config.maxBarsData || maxBars;
const screenWidth = config.screenWidth || 'unknown';

document.getElementById('barsToShow').max = maxBars;

// Create more informative help text
let helpText = `Max bars: ${maxBars}`;
if (maxBarsScreen !== maxBarsData) {
    const limitation = maxBarsScreen < maxBarsData ? 'screen resolution' : 'data buffer';
    helpText += ` (limited by ${limitation})`;
}
helpText += `\nScreen: ${screenWidth}px, Screen limit: ${maxBarsScreen}, Data limit: ${maxBarsData}`;

document.getElementById('barsHelpText').textContent = helpText;

// Show warning if data buffer is the limiting factor
if (maxBarsData < maxBarsScreen) {
    document.getElementById('barsWarning').style.display = 'block';
}

document.getElementById('useTestData').checked = config.useTestData || false;
// Remove intraday checkbox since it's always enabled
document.getElementById('enforceHours').checked = config.enforceHours !== false;

document.getElementById('useStaticIP').checked = config.useStaticIP || false;
document.getElementById('staticIP').value = config.staticIP || '192.168.4.184';
document.getElementById('gatewayIP').value = config.gatewayIP || '192.168.4.1';
document.getElementById('subnetMask').value = config.subnetMask || '255.255.255.0';

const staticFields = document.getElementById('staticIPFields');
staticFields.style.display = document.getElementById('useStaticIP').checked ? 'block' : 'none';

}catch(e){console.log('Load config error:',e)}
}

// Add interval change handler to update computed duration display
document.getElementById('yahooInterval').addEventListener('change', function() {
    // Update the displayed duration when interval changes
    const intervalToDuration = {
        '1m': 60,
        '2m': 120,
        '5m': 300,
        '15m': 900,
        '30m': 1800,
        '60m': 3600,
        '1h': 3600,
        '90m': 5400,
        '1d': 86400,
        '5d': 432000,
        '1wk': 604800,
        '1mo': 2592000,
        '3mo': 7776000
    };
    
    const duration = intervalToDuration[this.value] || 300; // Default to 5 minutes
    document.getElementById('computedDuration').textContent = duration;
});

document.getElementById('configForm').addEventListener('submit', async function(e){
e.preventDefault();

// Get and validate symbol
let symbol = document.getElementById('symbol').value.trim().toUpperCase();
if (!symbol) {
    showStatus('Please enter a stock symbol', 'error');
    return;
}
if (symbol.length > 6) {
    symbol = symbol.substring(0, 6);
}
// Basic validation: must contain at least one letter
if (!/[A-Z]/.test(symbol)) {
    showStatus('Symbol must contain at least one letter', 'error');
    return;
}

// Validate bars to show
const barsToShow = parseInt(document.getElementById('barsToShow').value);
const maxBars = parseInt(document.getElementById('barsToShow').max);
if (barsToShow > maxBars) {
    showStatus(`Bars to show cannot exceed ${maxBars} (limited by device constraints)`, 'error');
    return;
}

const config = {
symbol: symbol,
yahooInterval: document.getElementById('yahooInterval').value,
yahooRange: document.getElementById('yahooRange').value,
updateInterval: parseInt(document.getElementById('updateInterval').value),
// Remove: candleDuration: parseInt(document.getElementById('candleDuration').value),
barsToShow: barsToShow,
useTestData: document.getElementById('useTestData').checked,
// Remove: useIntraday: document.getElementById('useIntraday').checked,
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
    let message = 'Updated successfully! Candle duration auto-synced with interval.';
    if (config.useStaticIP) {
        message += ' Device restart required for network changes.';
    }
    // Reload config to get updated computed duration
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