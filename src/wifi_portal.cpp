#include "wifi_portal.h"
#include "led_control.h"
#include "statistics.h"
#include "alerts.h"
#ifdef BASE_STATION
#include "mqtt_client.h"
#include "sensor_config.h"
#include "remote_config.h"
#endif
#include <AsyncWebSocket.h>
#include <LittleFS.h>

// Global instance
WiFiPortal wifiPortal;

// WebSocket for live updates
AsyncWebSocket ws("/ws");

// DNS server port
const byte DNS_PORT = 53;

WiFiPortal::WiFiPortal() : webServer(80), portalActive(false), dashboardActive(false) {
}

void WiFiPortal::startPortal() {
    Serial.println("Starting WiFi Captive Portal...");
    
    // Get MAC address for unique SSID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apName[32];
    
    DeviceMode mode = configStorage.getDeviceMode();
    if (mode == MODE_SENSOR || mode == MODE_UNCONFIGURED) {
        snprintf(apName, sizeof(apName), "LoRa-Sensor-%02X%02X", mac[4], mac[5]);
    } else {
        snprintf(apName, sizeof(apName), "LoRa-Base-%02X%02X", mac[4], mac[5]);
    }
    
    // Start AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, "configure");
    
    IPAddress apIP(10, 8, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, subnet);
    
    Serial.print("AP Started: ");
    Serial.println(apName);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    
    // Set LED to blue for AP mode
    setLEDColor(0, 0, 255);
    
    // Start DNS server for captive portal redirect
    dnsServer.start(DNS_PORT, "*", apIP);
    
    // Setup web server routes
    setupWebServer();
    webServer.begin();
    
    portalActive = true;
}

void WiFiPortal::setupWebServer() {
    // Serve main page
    webServer.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateMainPage());
    });
    
    // Captive portal detection
    webServer.on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateMainPage());
    });
    webServer.on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateMainPage());
    });
    
    // Mode selection
    webServer.on("/mode", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleModeSelection(request);
    });
    
    // Sensor configuration page
    webServer.on("/sensor", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateSensorConfigPage());
    });
    
    webServer.on("/sensor", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSensorConfig(request);
    });
    
    // Base station configuration page
    webServer.on("/base", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateBaseStationConfigPage());
    });
    
    webServer.on("/base", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleBaseStationConfig(request);
    });
    
    // Catch-all for captive portal
    webServer.onNotFound([this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateMainPage());
    });
}

String WiFiPortal::generateMainPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LoRa Sensor Station Setup</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .mode-card {
            border: 2px solid #e0e0e0;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            cursor: pointer;
            transition: all 0.3s;
            background: #f9f9f9;
        }
        .mode-card:hover {
            border-color: #667eea;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102,126,234,0.2);
        }
        .mode-card h3 {
            color: #333;
            margin-bottom: 10px;
            font-size: 20px;
        }
        .mode-card p {
            color: #666;
            line-height: 1.6;
            font-size: 14px;
        }
        .btn {
            width: 100%;
            padding: 15px;
            border: none;
            border-radius: 8px;
            background: #667eea;
            color: white;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.3s;
            margin-top: 10px;
        }
        .btn:hover {
            background: #5568d3;
        }
        .info {
            text-align: center;
            color: #999;
            font-size: 12px;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎯 LoRa Sensor Station</h1>
        <p class="subtitle">Choose your device mode</p>
        
        <form action="/mode" method="POST">
            <div class="mode-card" onclick="this.querySelector('input').checked=true">
                <input type="radio" name="mode" value="sensor" style="display:none">
                <h3>📡 Sensor Client</h3>
                <p>Configure as a remote sensor node that transmits temperature and battery data to the base station.</p>
                <button type="submit" class="btn" onclick="this.form.mode.value='sensor'">Configure as Sensor</button>
            </div>
            
            <div class="mode-card" onclick="this.querySelector('input').checked=true">
                <input type="radio" name="mode" value="base" style="display:none">
                <h3>🏠 Base Station</h3>
                <p>Configure as the central hub that receives data from all sensors and provides web interface.</p>
                <button type="submit" class="btn" onclick="this.form.mode.value='base'">Configure as Base Station</button>
            </div>
        </form>
        
        <p class="info">Hold USER button for 10s to reset configuration</p>
    </div>
</body>
</html>
)rawliteral";
    return html;
}

String WiFiPortal::generateSensorConfigPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sensor Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 30px;
            font-size: 24px;
        }
        .form-group {
            margin-bottom: 25px;
        }
        label {
            display: block;
            color: #333;
            font-weight: 600;
            margin-bottom: 8px;
            font-size: 14px;
        }
        input, select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 15px;
            transition: border-color 0.3s;
        }
        input:focus, select:focus {
            outline: none;
            border-color: #667eea;
        }
        .btn {
            width: 100%;
            padding: 15px;
            border: none;
            border-radius: 8px;
            background: #667eea;
            color: white;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.3s;
        }
        .btn:hover {
            background: #5568d3;
        }
        .help-text {
            color: #999;
            font-size: 12px;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📡 Client Configuration</h1>
        <form action="/sensor" method="POST">
            <div class="form-group">
                <label for="sensorId">Client ID (1-255)</label>
                <input type="number" id="sensorId" name="sensorId" min="1" max="255" required>
                <p class="help-text">Unique identifier for this client device</p>
            </div>
            
            <div class="form-group">
                <label for="location">Location / Name</label>
                <input type="text" id="location" name="location" maxlength="31" placeholder="e.g., Living Room, Garage" required>
                <p class="help-text">Descriptive name for the sensor location</p>
            </div>
            
            <div class="form-group">
                <label for="networkId">Network ID (1-65535)</label>
                <input type="number" id="networkId" name="networkId" min="1" max="65535" value="12345" required>
                <p class="help-text">🔒 All devices in the same network must use the same Network ID</p>
            </div>
            
            <div class="form-group">
                <label for="interval">Transmission Interval</label>
                <select id="interval" name="interval" required>
                    <option value="15">Every 15 seconds</option>
                    <option value="30" selected>Every 30 seconds</option>
                    <option value="60">Every 60 seconds</option>
                    <option value="300">Every 5 minutes</option>
                </select>
                <p class="help-text">How often to send data to base station</p>
            </div>
            
            <button type="submit" class="btn">💾 Save & Reboot</button>
        </form>
    </div>
</body>
</html>
)rawliteral";
    return html;
}

String WiFiPortal::generateBaseStationConfigPage() {
    // Scan for WiFi networks with timeout
    String networkOptions = "";
    
    // Start async scan
    int n = WiFi.scanNetworks(false, false, false, 300);
    
    if (n > 0) {
        for (int i = 0; i < n && i < 20; i++) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓" : "🔒";
            
            networkOptions += "<option value=\"" + ssid + "\">";
            networkOptions += security + " " + ssid + " (" + String(rssi) + " dBm)";
            networkOptions += "</option>";
        }
    } else {
        networkOptions = "<option value=\"\">No networks found - enter manually</option>";
    }
    
    String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Base Station Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 30px;
            font-size: 24px;
        }
        .form-group {
            margin-bottom: 25px;
        }
        label {
            display: block;
            color: #333;
            font-weight: 600;
            margin-bottom: 8px;
            font-size: 14px;
        }
        input, select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 15px;
            transition: border-color 0.3s;
        }
        input:focus, select:focus {
            outline: none;
            border-color: #667eea;
        }
        .btn {
            width: 100%;
            padding: 15px;
            border: none;
            border-radius: 8px;
            background: #667eea;
            color: white;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.3s;
        }
        .btn:hover {
            background: #5568d3;
        }
        .help-text {
            color: #999;
            font-size: 12px;
            margin-top: 5px;
        }
        .status {
            text-align: center;
            padding: 15px;
            border-radius: 8px;
            margin-top: 20px;
            display: none;
        }
        .status.testing {
            background: #fff3cd;
            color: #856404;
            display: block;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏠 Base Station Configuration</h1>
        <form action="/base" method="POST" id="wifiForm">
            <div class="form-group">
                <label for="ssid">WiFi Network</label>
                <select id="ssid" name="ssid" required>
                    <option value="">Select a network...</option>
)rawliteral";
    
    html += networkOptions;
    
    html += R"rawliteral(
                </select>
                <p class="help-text">Available networks (refreshed on page load)</p>
            </div>
            
            <div class="form-group">
                <label for="password">WiFi Password</label>
                <input type="password" id="password" name="password">
                <p class="help-text">Enter the password for the selected network</p>
            </div>
            
            <div class="form-group">
                <label for="networkId">Network ID (1-65535)</label>
                <input type="number" id="networkId" name="networkId" min="1" max="65535" value="12345" required>
                <p class="help-text">🔒 Must match all client devices in your network</p>
            </div>
            
            <button type="submit" class="btn">🔌 Connect & Configure</button>
        </form>
        
        <div class="status" id="status">
            Testing connection...
        </div>
    </div>
    
    <script>
        document.getElementById('wifiForm').addEventListener('submit', function() {
            document.getElementById('status').className = 'status testing';
        });
    </script>
</body>
</html>
)rawliteral";
    return html;
}

String WiFiPortal::generateSuccessPage(const String& message) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration Saved</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            text-align: center;
        }
        .success-icon {
            font-size: 64px;
            margin-bottom: 20px;
        }
        h1 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 24px;
        }
        p {
            color: #666;
            line-height: 1.6;
            margin-bottom: 20px;
        }
        .countdown {
            font-size: 48px;
            color: #667eea;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="success-icon">✅</div>
        <h1>Configuration Saved!</h1>
        <p>)rawliteral" + message + R"rawliteral(</p>
        <div class="countdown" id="countdown">3</div>
    </div>
    
    <script>
        let seconds = 3;
        const countdownEl = document.getElementById('countdown');
        const interval = setInterval(() => {
            seconds--;
            countdownEl.textContent = seconds;
            if (seconds <= 0) {
                clearInterval(interval);
                countdownEl.textContent = 'Rebooting...';
            }
        }, 1000);
    </script>
</body>
</html>
)rawliteral";
    return html;
}

void WiFiPortal::handleModeSelection(AsyncWebServerRequest *request) {
    if (request->hasParam("mode", true)) {
        String mode = request->getParam("mode", true)->value();
        
        if (mode == "sensor") {
            request->redirect("/sensor");
        } else if (mode == "base") {
            request->redirect("/base");
        } else {
            request->send(400, "text/html", "Invalid mode");
        }
    } else {
        request->send(400, "text/html", "No mode specified");
    }
}

void WiFiPortal::handleSensorConfig(AsyncWebServerRequest *request) {
    if (request->hasParam("sensorId", true) && 
        request->hasParam("location", true) && 
        request->hasParam("interval", true) &&
        request->hasParam("networkId", true)) {
        
        SensorConfig config;
        config.sensorId = request->getParam("sensorId", true)->value().toInt();
        strncpy(config.location, request->getParam("location", true)->value().c_str(), sizeof(config.location) - 1);
        config.location[sizeof(config.location) - 1] = '\0';
        config.transmitInterval = request->getParam("interval", true)->value().toInt();
        config.networkId = request->getParam("networkId", true)->value().toInt();
        config.configured = true;
        
        // Save configuration
        configStorage.setSensorConfig(config);
        configStorage.setDeviceMode(MODE_SENSOR);
        
        Serial.println("Sensor configuration saved:");
        Serial.printf("  ID: %d\n", config.sensorId);
        Serial.printf("  Location: %s\n", config.location);
        Serial.printf("  Interval: %d seconds\n", config.transmitInterval);
        Serial.printf("  Network ID: %d\n", config.networkId);
        
        // Send success page
        String message = "Sensor ID " + String(config.sensorId) + " configured.<br>Device will reboot and start transmitting data.";
        request->send(200, "text/html", generateSuccessPage(message));
        
        // Schedule reboot
        delay(3000);
        ESP.restart();
    } else {
        request->send(400, "text/html", "Missing required fields");
    }
}

void WiFiPortal::handleBaseStationConfig(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true) && request->hasParam("networkId", true)) {
        BaseStationConfig config;
        strncpy(config.ssid, request->getParam("ssid", true)->value().c_str(), sizeof(config.ssid) - 1);
        config.ssid[sizeof(config.ssid) - 1] = '\0';
        strncpy(config.password, request->getParam("password", true)->value().c_str(), sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
        config.networkId = request->getParam("networkId", true)->value().toInt();
        config.configured = true;
        
        Serial.println("Testing WiFi connection...");
        Serial.printf("  SSID: %s\n", config.ssid);
        Serial.printf("  Network ID: %d\n", config.networkId);
        
        // Test connection
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.password);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connection successful!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            
            // Save configuration
            configStorage.setBaseStationConfig(config);
            configStorage.setDeviceMode(MODE_BASE_STATION);
            
            String message = "Successfully connected to " + String(config.ssid) + ".<br>IP Address: " + WiFi.localIP().toString() + "<br>Device will reboot and start base station mode.";
            request->send(200, "text/html", generateSuccessPage(message));
            
            // Schedule reboot
            delay(3000);
            ESP.restart();
        } else {
            Serial.println("WiFi connection failed!");
            
            // Return to AP mode
            WiFi.mode(WIFI_AP);
            String message = "Failed to connect to " + String(config.ssid) + ".<br>Please check password and try again.";
            request->send(200, "text/html", generateSuccessPage(message));
        }
    } else {
        request->send(400, "text/html", "Missing required fields");
    }
}

bool WiFiPortal::connectToWiFi(const char* ssid, const char* password) {
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        setLEDColor(0, 255, 0);  // Green for connected
        return true;
    } else {
        Serial.println("WiFi Connection Failed!");
        return false;
    }
}

void WiFiPortal::handleClient() {
    if (portalActive) {
        dnsServer.processNextRequest();
    }
}

bool WiFiPortal::isPortalActive() {
    return portalActive;
}

bool WiFiPortal::isDashboardActive() {
    return dashboardActive;
}

String WiFiPortal::getAPIP() {
    return WiFi.softAPIP().toString();
}

// ============================================================================
// WEB DASHBOARD (BASE STATION MODE)
// ============================================================================

void WiFiPortal::startDashboard() {
    Serial.println("Starting Web Dashboard...");
    
    // Stop portal if running
    if (portalActive) {
        Serial.println("Stopping DNS server...");
        dnsServer.stop();
        portalActive = false;
    }
    
    // Setup dashboard routes (will add to existing server)
    setupDashboard();
    
    // Only start web server if not already started
    if (!dashboardActive && !portalActive) {
        Serial.println("Starting web server...");
        webServer.begin();
        Serial.println("Web server started");
    } else {
        Serial.println("Web server already running, routes updated");
    }
    
    Serial.print("Dashboard available at: http://");
    Serial.println(WiFi.localIP());
    
    dashboardActive = true;
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // Send initial data to newly connected client
            client->text(wifiPortal.generateSensorsJSON());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // Handle incoming WebSocket messages if needed
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WiFiPortal::setupDashboard() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed! Dashboard will not work.");
        return;
    }
    Serial.println("LittleFS mounted successfully");
    
    // Setup WebSocket
    ws.onEvent(onWebSocketEvent);
    webServer.addHandler(&ws);
    Serial.println("WebSocket server started at /ws");
    
    // Handle /dashboard explicitly (redirect to root or serve dashboard.html)
    webServer.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/dashboard.html")) {
            request->send(LittleFS, "/dashboard.html", "text/html");
        } else {
            request->send(404, "text/plain", "Dashboard not found");
        }
    });
    
    // API endpoints - MUST be registered BEFORE serveStatic!
    webServer.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateSensorsJSON());
    });
    
    webServer.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateStatsJSON());
    });
    
    // Historical data endpoint
    webServer.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!request->hasParam("sensorId")) {
            request->send(400, "application/json", "{\"error\":\"sensorId parameter required\"}");
            return;
        }
        
        uint8_t sensorId = request->getParam("sensorId")->value().toInt();
        uint32_t timeRange = 0;  // 0 = all data
        
        if (request->hasParam("range")) {
            String range = request->getParam("range")->value();
            if (range == "1h") timeRange = 3600;
            else if (range == "6h") timeRange = 21600;
            else if (range == "24h") timeRange = 86400;
        }
        
        request->send(200, "application/json", generateHistoryJSON(sensorId, timeRange));
    });
    
    // Export endpoints
    webServer.on("/export/csv", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String csv = "Sensor ID,Location,Temperature,Battery,RSSI,Last Seen\n";
        
        for (int i = 0; i < 10; i++) {
            SensorInfo* sensor = getSensorByIndex(i);
            if (sensor != NULL) {
                uint32_t ageSeconds = (millis() - sensor->lastSeen) / 1000;
                csv += String(sensor->sensorId) + ",";
                csv += "Sensor " + String(sensor->sensorId) + ",";
                csv += String(sensor->lastTemperature, 1) + ",";
                csv += String(sensor->lastBatteryPercent) + "%,";
                csv += String(sensor->lastRssi) + ",";
                csv += String(ageSeconds) + "s ago\n";
            }
        }
        
        request->send(200, "text/csv", csv);
    });
    
    webServer.on("/export/json", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateSensorsJSON());
    });
    
    // Alerts configuration page
    webServer.on("/alerts", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateAlertsPage());
    });
    
    // Alerts API endpoints
    webServer.on("/api/alerts/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateAlertsConfigJSON());
    });
    
    webServer.on("/api/alerts/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleAlertsConfigUpdate(request, data, len);
        });
    
    webServer.on("/api/alerts/test", HTTP_POST, [this](AsyncWebServerRequest *request) {
        bool success = testTeamsWebhook();
        String response = success ? "{\"success\":true,\"message\":\"Test alert sent!\"}" : "{\"success\":false,\"message\":\"Failed to send test alert\"}";
        request->send(success ? 200 : 500, "application/json", response);
    });
    
    webServer.on("/api/alerts/test-email", HTTP_POST, [this](AsyncWebServerRequest *request) {
        bool success = alertManager.testEmailSettings();
        String response = success ? "{\"success\":true,\"message\":\"Test email sent!\"}" : "{\"success\":false,\"message\":\"Failed to send test email\"}";
        request->send(success ? 200 : 500, "application/json", response);
    });
    
    // MQTT configuration page
    #ifdef BASE_STATION
    webServer.on("/mqtt", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateMQTTPage());
    });
    
    // MQTT API endpoints
    webServer.on("/api/mqtt/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateMQTTConfigJSON());
    });
    
    webServer.on("/api/mqtt/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleMQTTConfigUpdate(request, data, len);
        });
    
    webServer.on("/api/mqtt/test", HTTP_POST, [this](AsyncWebServerRequest *request) {
        bool connected = mqttClient.isConnected();
        if (!connected) {
            connected = mqttClient.connect();
        }
        String response = connected ? "{\"success\":true,\"message\":\"MQTT connected successfully!\"}" : "{\"success\":false,\"message\":\"Failed to connect to MQTT broker\"}";
        request->send(connected ? 200 : 500, "application/json", response);
    });
    
    webServer.on("/api/mqtt/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"connected\":" + String(mqttClient.isConnected() ? "true" : "false") + ",";
        json += "\"publishes\":" + String(mqttClient.getPublishCount()) + ",";
        json += "\"failures\":" + String(mqttClient.getFailedPublishCount()) + ",";
        json += "\"reconnects\":" + String(mqttClient.getReconnectCount());
        json += "}";
        request->send(200, "application/json", json);
    });
    
    // Sensor Configuration Page
    webServer.on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateSensorNamesPage());
    });
    
    webServer.on("/api/sensor-names", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateSensorNamesJSON());
    });
    
    webServer.on("/api/sensor-names", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSensorNameUpdate(request, data, len);
        });
    
    // Remote configuration API
    webServer.on("/api/remote-config/interval", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            Serial.println("POST /api/remote-config/interval - request handler called (no body yet)");
        }, 
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            Serial.printf("Body callback: data=%p, len=%d, index=%d, total=%d\n", data, len, index, total);
            handleRemoteSetInterval(request, data, len);
        });
    
    webServer.on("/api/remote-config/restart", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleRemoteRestart(request, data, len);
        });
    
    webServer.on("/api/remote-config/get-config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleRemoteGetConfig(request, data, len);
        });
    
    webServer.on("/api/remote-config/queue-status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateCommandQueueJSON());
    });
    #endif // BASE_STATION
    
    // Handle favicon.ico with 404 (instead of 500 error)
    webServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });
    
    // Serve static files from LittleFS - MUST BE LAST (catch-all)
    webServer.serveStatic("/", LittleFS, "/").setDefaultFile("dashboard.html");
}

/**
 * @brief Broadcast sensor data to all connected WebSocket clients
 * 
 * Called when new sensor data is received to push updates to browser clients instantly
 */
void WiFiPortal::broadcastSensorUpdate() {
    if (!dashboardActive || ws.count() == 0) {
        return;  // No clients connected
    }
    
    String json = generateSensorsJSON();
    ws.textAll(json);
    Serial.printf("WebSocket broadcast to %d clients\n", ws.count());
}

String WiFiPortal::generateDashboardPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LoRa Sensor Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #f5f7fa;
            padding: 20px;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 15px;
            margin-bottom: 30px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .header h1 {
            font-size: 32px;
            margin-bottom: 10px;
        }
        .header-stats {
            display: flex;
            gap: 30px;
            margin-top: 20px;
            flex-wrap: wrap;
        }
        .header-stat {
            flex: 1;
            min-width: 150px;
        }
        .header-stat-label {
            font-size: 12px;
            opacity: 0.8;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .header-stat-value {
            font-size: 28px;
            font-weight: bold;
            margin-top: 5px;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
            transition: transform 0.2s;
        }
        .card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        }
        .card-title {
            font-size: 18px;
            font-weight: 600;
            color: #333;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .sensor-item {
            padding: 15px;
            border-left: 4px solid #667eea;
            background: #f9f9f9;
            margin-bottom: 12px;
            border-radius: 6px;
        }
        .sensor-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
        }
        .sensor-id {
            font-weight: 600;
            color: #667eea;
            font-size: 16px;
        }
        .sensor-time {
            font-size: 12px;
            color: #999;
        }
        .sensor-data {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin-top: 10px;
        }
        .sensor-metric {
            text-align: center;
        }
        .sensor-metric-label {
            font-size: 11px;
            color: #666;
            text-transform: uppercase;
        }
        .sensor-metric-value {
            font-size: 18px;
            font-weight: 600;
            color: #333;
            margin-top: 2px;
        }
        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-online { background: #10b981; }
        .status-warning { background: #f59e0b; }
        .status-offline { background: #ef4444; }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
        }
        .stat-item {
            background: #f9f9f9;
            padding: 15px;
            border-radius: 8px;
        }
        .stat-label {
            font-size: 12px;
            color: #666;
            text-transform: uppercase;
            margin-bottom: 5px;
        }
        .stat-value {
            font-size: 24px;
            font-weight: bold;
            color: #333;
        }
        .export-buttons {
            display: flex;
            gap: 10px;
            margin-top: 20px;
        }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 6px;
            background: #667eea;
            color: white;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.3s;
            text-decoration: none;
            display: inline-block;
        }
        .btn:hover {
            background: #5568d3;
        }
        .btn-secondary {
            background: #6b7280;
        }
        .btn-secondary:hover {
            background: #4b5563;
        }
        .time-btn {
            padding: 8px 16px;
            border: 2px solid #667eea;
            background: white;
            color: #667eea;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 600;
            transition: all 0.3s;
        }
        .time-btn:hover {
            background: #f3f4f6;
        }
        .time-btn.active {
            background: #667eea;
            color: white;
        }
        .empty-state {
            text-align: center;
            padding: 40px;
            color: #999;
        }
        .empty-state-icon {
            font-size: 48px;
            margin-bottom: 15px;
        }
        @media (max-width: 768px) {
            .header-stats {
                flex-direction: column;
            }
            .sensor-data {
                grid-template-columns: 1fr;
            }
        }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🎯 LoRa Sensor Dashboard</h1>
            <p>Real-time monitoring and data visualization</p>
            <div class="header-stats">
                <div class="header-stat">
                    <div class="header-stat-label">Connection</div>
                    <div class="header-stat-value" id="wsStatus" style="font-size: 14px;">🔵 Connecting...</div>
                </div>
                <div class="header-stat">
                    <div class="header-stat-label">Active Clients</div>
                    <div class="header-stat-value" id="activeSensorCount">0</div>
                </div>
                <div class="header-stat">
                    <div class="header-stat-label">Total Packets</div>
                    <div class="header-stat-value" id="totalPackets">0</div>
                </div>
                <div class="header-stat">
                    <div class="header-stat-label">RX Invalid</div>
                    <div class="header-stat-value" id="rxInvalid">0</div>
                </div>
                <div class="header-stat">
                    <div class="header-stat-label">Success Rate</div>
                    <div class="header-stat-value" id="successRate">0%</div>
                </div>
            </div>
        </div>
        
        <div class="grid">
            <div class="card" style="grid-column: 1 / -1;">
                <div class="card-title">
                    📡 Active Clients
                    <div style="margin-left: auto; font-size: 12px; color: #999;">
                        Auto-refresh: <span id="countdown">5</span>s
                    </div>
                </div>
                <div id="sensorList">
                    <div class="empty-state">
                        <div class="empty-state-icon">📭</div>
                        <p>No clients detected yet</p>
                        <p style="font-size: 12px; margin-top: 10px;">Waiting for client data...</p>
                    </div>
                </div>
            </div>
            
            <div class="card">
                <div class="card-title">💾 Data Export</div>
                <p style="color: #666; margin-bottom: 15px; font-size: 14px;">
                    Download sensor data for analysis
                </p>
                <div class="export-buttons">
                    <a href="/export/csv" class="btn" download>📄 Export CSV</a>
                    <a href="/export/json" class="btn btn-secondary" download>📋 Export JSON</a>
                </div>
            </div>
            
            <div class="card">
                <div class="card-title">🔔 Alerts & Notifications</div>
                <p style="color: #666; margin-bottom: 15px; font-size: 14px;">
                    Configure Teams notifications and thresholds
                </p>
                <div class="export-buttons">
                    <a href="/alerts" class="btn">⚙️ Configure Alerts</a>
                </div>
            </div>
            
            <div class="card">
                <div class="card-title">🏷️ Sensor Configuration</div>
                <p style="color: #666; margin-bottom: 15px; font-size: 14px;">
                    Set friendly names and locations for your sensors
                </p>
                <div class="export-buttons">
                    <a href="/sensors" class="btn">⚙️ Configure Sensors</a>
                </div>
            </div>
            
            <div class="card">
                <div class="card-title">📡 MQTT Publishing</div>
                <p style="color: #666; margin-bottom: 15px; font-size: 14px;">
                    Configure MQTT broker and Home Assistant integration
                </p>
                <div class="export-buttons">
                    <a href="/mqtt" class="btn">⚙️ Configure MQTT</a>
                </div>
            </div>
        </div>
        
        <!-- Historical Data Graphs -->
        <div class="card" style="margin-bottom: 20px;">
            <div class="card-title">
                📈 Historical Data - Select Client
            </div>
            <div style="margin-bottom: 20px;">
                <label for="sensorSelect" style="display: block; margin-bottom: 8px; font-weight: 600;">Client:</label>
                <select id="sensorSelect" style="padding: 8px; border-radius: 6px; border: 2px solid #e0e0e0; font-size: 14px;">
                    <option value="">-- Select a client --</option>
                </select>
                
                <label for="timeRange" style="display: block; margin: 15px 0 8px; font-weight: 600;">Time Range:</label>
                <div style="display: flex; gap: 10px;">
                    <button class="time-btn active" data-range="all">All</button>
                    <button class="time-btn" data-range="1h">1 Hour</button>
                    <button class="time-btn" data-range="6h">6 Hours</button>
                    <button class="time-btn" data-range="24h">24 Hours</button>
                </div>
            </div>
        </div>
        
        <div class="grid">
            <div class="card" style="grid-column: 1 / -1;">
                <div class="card-title">🌡️ Temperature History</div>
                <canvas id="tempChart" style="max-height: 300px;"></canvas>
            </div>
            
            <div class="card" style="grid-column: 1 / -1;">
                <div class="card-title">🔋 Battery History</div>
                <canvas id="battChart" style="max-height: 300px;"></canvas>
            </div>
            
            <div class="card" style="grid-column: 1 / -1;">
                <div class="card-title">📶 Signal Strength (RSSI)</div>
                <canvas id="rssiChart" style="max-height: 300px;"></canvas>
            </div>
        </div>
    </div>
    
    <script>
        let countdown = 5;
        
        // Chart.js configuration - declare variables at the top
        let tempChart, battChart, rssiChart;
        let currentSensorId = null;
        let currentTimeRange = 'all';
        let inactivityTimeoutMinutes = 15;  // Default, will be loaded from config
        
        // WebSocket connection
        let ws = null;
        let wsReconnectTimer = null;
        let wsConnected = false;
        
        // Initialize WebSocket connection
        function initWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;
            
            try {
                ws = new WebSocket(wsUrl);
                
                ws.onopen = () => {
                    console.log('WebSocket connected');
                    wsConnected = true;
                    updateConnectionStatus(true);
                    
                    // Clear any reconnect timers
                    if (wsReconnectTimer) {
                        clearTimeout(wsReconnectTimer);
                        wsReconnectTimer = null;
                    }
                };
                
                ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        // Real-time sensor update received
                        updateSensorList(data);
                        console.log('WebSocket update received:', data.length, 'sensors');
                    } catch (error) {
                        console.error('Error parsing WebSocket message:', error);
                    }
                };
                
                ws.onerror = (error) => {
                    console.error('WebSocket error:', error);
                    updateConnectionStatus(false);
                };
                
                ws.onclose = () => {
                    console.log('WebSocket disconnected');
                    wsConnected = false;
                    updateConnectionStatus(false);
                    
                    // Attempt to reconnect after 3 seconds
                    wsReconnectTimer = setTimeout(() => {
                        console.log('Attempting to reconnect WebSocket...');
                        initWebSocket();
                    }, 3000);
                };
            } catch (error) {
                console.error('Failed to create WebSocket:', error);
                // Fallback to polling if WebSocket fails
                startPolling();
            }
        }
        
        // Update connection status indicator
        function updateConnectionStatus(connected) {
            const statusEl = document.getElementById('wsStatus');
            if (statusEl) {
                statusEl.textContent = connected ? '🟢 Live' : '🔴 Reconnecting...';
                statusEl.style.color = connected ? '#10b981' : '#ef4444';
            }
        }
        
        // Fallback polling if WebSocket fails
        function startPolling() {
            setInterval(updateDashboard, 5000);
        }
        
        // Load alert configuration for inactivity timeout
        async function loadAlertConfig() {
            try {
                const response = await fetch('/api/alerts/config');
                const config = await response.json();
                inactivityTimeoutMinutes = config.timeout || 15;
            } catch (error) {
                console.error('Error loading alert config:', error);
            }
        }
        
        function updateDashboard() {
            // Only use HTTP fallback if WebSocket is not connected
            if (!wsConnected) {
                // Fetch sensor data
                fetch('/api/sensors')
                    .then(response => response.json())
                    .then(data => {
                        updateSensorList(data);
                    })
                    .catch(error => console.error('Error fetching sensors:', error));
            }
            
            // Always fetch stats (not pushed via WebSocket)
            fetch('/api/stats')
                .then(response => response.json())
                .then(data => {
                    updateStats(data);
                })
                .catch(error => console.error('Error fetching stats:', error));
            
            // Auto-refresh charts if a sensor is selected
            if (currentSensorId) {
                loadChartData();
            }
            
            countdown = 5;
        }
        
        function updateSensorList(sensors) {
            const container = document.getElementById('sensorList');
            
            if (sensors.length === 0) {
                container.innerHTML = `
                    <div class="empty-state">
                        <div class="empty-state-icon">📭</div>
                        <p>No clients detected yet</p>
                        <p style="font-size: 12px; margin-top: 10px;">Waiting for client data...</p>
                    </div>
                `;
                return;
            }
            
            let html = '';
            sensors.forEach(sensor => {
                const inactivityTimeoutSeconds = inactivityTimeoutMinutes * 60;
                const isInactive = sensor.ageSeconds >= inactivityTimeoutSeconds;
                
                const statusClass = sensor.ageSeconds < 300 ? 'status-online' : 
                                  sensor.ageSeconds < 900 ? 'status-warning' : 'status-offline';
                
                const batteryColor = sensor.battery > 80 ? '#10b981' : 
                                   sensor.battery > 50 ? '#f59e0b' : 
                                   sensor.battery > 20 ? '#fb923c' : '#ef4444';
                
                const signalColor = sensor.rssi > -60 ? '#10b981' : 
                                   sensor.rssi > -80 ? '#f59e0b' : 
                                   sensor.rssi > -100 ? '#fb923c' : '#ef4444';
                
                // Add warning banner for inactive clients
                const warningBanner = isInactive ? `
                    <div style="background: #fff3cd; border: 2px solid #ffc107; border-radius: 6px; padding: 10px; margin-bottom: 10px;">
                        <span style="color: #856404; font-weight: 600;">⚠️ Client Inactive</span>
                        <span style="color: #856404; font-size: 12px; margin-left: 8px;">No data received for ${Math.floor(sensor.ageSeconds / 60)} minutes</span>
                    </div>
                ` : '';
                
                // Client header with battery and radio status
                html += `
                    <div class="sensor-item" style="${isInactive ? 'border: 2px solid #ffc107; background: #fff9e6;' : ''}">
                        ${warningBanner}
                        <div class="sensor-header" style="border-bottom: 2px solid #e5e7eb; padding-bottom: 12px; margin-bottom: 12px;">
                            <div style="display: flex; align-items: center; gap: 12px;">
                                <span class="status-indicator ${statusClass}"></span>
                                <span class="sensor-id" style="font-size: 16px; font-weight: 600;">${sensor.location || 'Client #' + sensor.id}</span>
                                <span style="background: ${batteryColor}; color: white; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 600;">
                                    🔋 ${sensor.battery}%
                                </span>
                                <span style="background: ${signalColor}; color: white; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 600;">
                                    📡 ${sensor.rssi} dBm
                                </span>
                            </div>
                            <span class="sensor-time">${sensor.age}</span>
                        </div>
                        
                        <div style="padding-left: 28px;">
                            ${sensor.sensorCount > 0 ? `
                                <div style="font-size: 12px; color: #6b7280; margin-bottom: 8px; font-weight: 500;">
                                    Connected Sensors (${sensor.sensorCount}):
                                </div>
                                <div style="display: flex; flex-direction: column; gap: 6px;">
                                    ${sensor.temperature > -127 ? `
                                        <div style="background: #f3f4f6; padding: 8px 12px; border-radius: 6px; display: flex; justify-content: space-between; align-items: center;">
                                            <span style="font-size: 13px; color: #374151;">🌡️ Temperature Sensor</span>
                                            <span style="font-size: 14px; font-weight: 600; color: #1f2937;">${sensor.temperature}°C</span>
                                        </div>
                                    ` : ''}
                                </div>
                            ` : `
                                <div style="font-size: 13px; color: #9ca3af; font-style: italic; padding: 8px 0;">
                                    No sensors configured on this client
                                </div>
                            `}
                        </div>
                    </div>
                `;
            });
            
            container.innerHTML = html;
            
            // Update sensor dropdown for charts
            updateSensorSelect(sensors);
        }
        
        function updateStats(stats) {
            document.getElementById('activeSensorCount').textContent = stats.activeSensors;
            document.getElementById('totalPackets').textContent = stats.totalRx;
            document.getElementById('rxInvalid').textContent = stats.totalInvalid;
            document.getElementById('successRate').textContent = stats.successRate + '%';
        }
        
        // Update countdown timer
        setInterval(() => {
            countdown--;
            document.getElementById('countdown').textContent = countdown;
            
            if (countdown <= 0) {
                updateDashboard();
                countdown = 5;
            }
        }, 1000);
        
        // Initial load
        loadAlertConfig();  // Load inactivity timeout setting
        initWebSocket();    // Initialize WebSocket for real-time updates
        updateDashboard();  // Initial data fetch (and fallback if WS fails)
        
        // Initialize charts
        const chartConfig = (label, color, unit = '') => ({
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: label,
                    data: [],
                    borderColor: color,
                    backgroundColor: color + '20',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4,
                    pointRadius: 3,
                    pointHoverRadius: 5
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                interaction: {
                    intersect: false,
                    mode: 'index'
                },
                plugins: {
                    legend: {
                        display: true,
                        position: 'top'
                    },
                    tooltip: {
                        callbacks: {
                            label: function(context) {
                                return context.dataset.label + ': ' + context.parsed.y + unit;
                            }
                        }
                    }
                },
                scales: {
                    x: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        display: true,
                        title: {
                            display: true,
                            text: label
                        }
                    }
                }
            }
        });
        
        tempChart = new Chart(document.getElementById('tempChart'), chartConfig('Temperature', '#ef4444', '°C'));
        battChart = new Chart(document.getElementById('battChart'), chartConfig('Battery', '#10b981', '%'));
        rssiChart = new Chart(document.getElementById('rssiChart'), chartConfig('RSSI', '#3b82f6', ' dBm'));
        
        // Update sensor select dropdown
        function updateSensorSelect(sensors) {
            const select = document.getElementById('sensorSelect');
            const currentValue = select.value;
            
            // Clear and repopulate
            select.innerHTML = '<option value="">-- Select a client --</option>';
            
            sensors.forEach(sensor => {
                const option = document.createElement('option');
                option.value = sensor.id;
                const label = sensor.location || ('Client ' + sensor.id);
                option.textContent = label + ' (' + sensor.temperature + '°C)';
                select.appendChild(option);
            });
            
            // Restore selection or select first sensor
            if (currentValue && sensors.find(s => s.id == currentValue)) {
                select.value = currentValue;
            } else if (sensors.length > 0 && !currentSensorId) {
                select.value = sensors[0].id;
                currentSensorId = sensors[0].id;
                loadChartData();
            }
        }
        
        // Load chart data from API
        function loadChartData() {
            if (!currentSensorId) return;
            
            const url = '/api/history?sensorId=' + currentSensorId + 
                       (currentTimeRange !== 'all' ? '&range=' + currentTimeRange : '');
            
            fetch(url)
                .then(response => response.json())
                .then(data => {
                    if (data.error) {
                        console.error('No data:', data.error);
                        return;
                    }
                    
                    updateCharts(data.data);
                })
                .catch(error => console.error('Error loading chart data:', error));
        }
        
        // Update all charts with new data
        function updateCharts(dataPoints) {
            const labels = [];
            const tempData = [];
            const battData = [];
            const rssiData = [];
            
            dataPoints.forEach(point => {
                // Convert timestamp to readable time
                const date = new Date(point.t * 1000);
                const timeStr = date.getHours().toString().padStart(2, '0') + ':' + 
                               date.getMinutes().toString().padStart(2, '0');
                labels.push(timeStr);
                
                tempData.push(point.temp);
                battData.push(point.batt);
                rssiData.push(point.rssi);
            });
            
            // Update temperature chart
            tempChart.data.labels = labels;
            tempChart.data.datasets[0].data = tempData;
            tempChart.update();
            
            // Update battery chart
            battChart.data.labels = labels;
            battChart.data.datasets[0].data = battData;
            battChart.update();
            
            // Update RSSI chart
            rssiChart.data.labels = labels;
            rssiChart.data.datasets[0].data = rssiData;
            rssiChart.update();
        }
        
        // Sensor selection change
        document.getElementById('sensorSelect').addEventListener('change', function() {
            currentSensorId = this.value;
            if (currentSensorId) {
                loadChartData();
            }
        });
        
        // Time range button handlers
        document.querySelectorAll('.time-btn').forEach(btn => {
            btn.addEventListener('click', function() {
                document.querySelectorAll('.time-btn').forEach(b => b.classList.remove('active'));
                this.classList.add('active');
                currentTimeRange = this.dataset.range;
                if (currentSensorId) {
                    loadChartData();
                }
            });
        });
    </script>
</body>
</html>
)rawliteral";
    return html;
}

String WiFiPortal::generateSensorsJSON() {
    String json = "[";
    bool first = true;
    
    for (int i = 0; i < 10; i++) {
        SensorInfo* sensor = getSensorByIndex(i);
        if (sensor != NULL) {
            if (!first) json += ",";
            first = false;
            
            uint32_t ageSeconds = (millis() - sensor->lastSeen) / 1000;
            String ageStr;
            if (ageSeconds < 60) {
                ageStr = String(ageSeconds) + "s ago";
            } else if (ageSeconds < 3600) {
                ageStr = String(ageSeconds / 60) + "m ago";
            } else {
                ageStr = String(ageSeconds / 3600) + "h ago";
            }
            
            // Count how many physical sensors are connected (temperature != -127 means sensor exists)
            uint8_t sensorCount = 0;
            if (sensor->lastTemperature > -127.0f) {
                sensorCount++;
            }
            
            json += "{";
            json += "\"id\":" + String(sensor->sensorId) + ",";
            json += "\"location\":\"" + String(sensor->location) + "\",";
            json += "\"battery\":" + String(sensor->lastBatteryPercent) + ",";
            json += "\"charging\":" + String(sensor->powerState ? "true" : "false") + ",";
            json += "\"rssi\":" + String(sensor->lastRssi) + ",";
            json += "\"snr\":" + String(sensor->lastSnr) + ",";
            json += "\"packets\":" + String(sensor->packetsReceived) + ",";
            json += "\"ageSeconds\":" + String(ageSeconds) + ",";
            json += "\"age\":\"" + ageStr + "\"";
            json += "}";
        }
    }
    
    json += "]";
    return json;
}

String WiFiPortal::generateStatsJSON() {
    SystemStats* stats = getStats();
    uint8_t activeClients = getActiveClientCount();  // Count clients, not physical sensors
    
    uint32_t successRate = 0;
    if (stats->totalRxPackets + stats->totalRxInvalid > 0) {
        successRate = (stats->totalRxPackets * 100) / (stats->totalRxPackets + stats->totalRxInvalid);
    }
    
    String json = "{";
    json += "\"activeSensors\":" + String(activeClients) + ",";  // Keep field name for compatibility
    json += "\"totalRx\":" + String(stats->totalRxPackets) + ",";
    json += "\"totalInvalid\":" + String(stats->totalRxInvalid) + ",";
    json += "\"successRate\":" + String(successRate) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    
    return json;
}

String WiFiPortal::generateHistoryJSON(uint8_t sensorId, uint32_t timeRange) {
    // For backward compatibility, treat sensorId as clientId
    // Return client telemetry history (battery/RSSI over time)
    ClientHistory* history = getClientHistory(sensorId);
    
    Serial.printf("📈 API /api/history: clientId=%d, history=%p\n", sensorId, history);
    
    if (history == NULL || history->count == 0) {
        Serial.printf("📈 History %s (count=%d)\n", 
                     history == NULL ? "is NULL" : "has zero count",
                     history ? history->count : 0);
        return "{\"error\":\"No data available\",\"data\":[]}";
    }
    
    Serial.printf("📈 History found: count=%d, index=%d\n", history->count, history->index);
    
    uint32_t currentTime = millis() / 1000;
    uint32_t cutoffTime = (timeRange > 0) ? (currentTime - timeRange) : 0;
    
    Serial.printf("📈 Time filtering: current=%u, range=%u, cutoff=%u\n", 
                 currentTime, timeRange, cutoffTime);
    
    String json = "{\"sensorId\":" + String(sensorId) + ",\"data\":[";
    bool first = true;
    int skipped = 0;
    int included = 0;
    
    // Iterate through ring buffer in chronological order
    uint16_t startIdx = (history->count < HISTORY_SIZE) ? 0 : history->index;
    uint16_t count = (history->count < HISTORY_SIZE) ? history->count : HISTORY_SIZE;
    
    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (startIdx + i) % HISTORY_SIZE;
        ClientDataPoint* point = &history->data[idx];
        
        Serial.printf("📈 Point[%d]: ts=%u, batt=%d%%, rssi=%d dBm, charging=%s\n",
                     idx, point->timestamp, point->battery, point->rssi, point->charging ? "YES" : "NO");
        
        // Skip if outside time range
        if (timeRange > 0 && point->timestamp < cutoffTime) {
            Serial.printf("📈 SKIPPED (ts %u < cutoff %u)\n", point->timestamp, cutoffTime);
            skipped++;
            continue;
        }
        
        included++;
        
        if (!first) json += ",";
        first = false;
        
        json += "{";
        json += "\"t\":" + String(point->timestamp) + ",";
        json += "\"batt\":" + String(point->battery) + ",";
        json += "\"rssi\":" + String(point->rssi) + ",";
        json += "\"charging\":" + String(point->charging ? "true" : "false");
        json += "}";
    }
    
    Serial.printf("📈 Result: included=%d, skipped=%d, total=%d\n", included, skipped, count);
    
    json += "]}";
    return json;
}

String WiFiPortal::generateAlertsConfigJSON() {
    AlertConfig* config = alertManager.getConfig();
    
    String json = "{";
    json += "\"teamsEnabled\":" + String(config->teamsEnabled ? "true" : "false") + ",";
    json += "\"teamsWebhook\":\"" + String(config->teamsWebhook) + "\",";
    json += "\"emailEnabled\":" + String(config->emailEnabled ? "true" : "false") + ",";
    json += "\"smtpServer\":\"" + String(config->smtpServer) + "\",";
    json += "\"smtpPort\":" + String(config->smtpPort) + ",";
    json += "\"emailUser\":\"" + String(config->emailUser) + "\",";
    json += "\"emailPassword\":\"" + String(config->emailPassword) + "\",";
    json += "\"emailFrom\":\"" + String(config->emailFrom) + "\",";
    json += "\"emailTo\":\"" + String(config->emailTo) + "\",";
    json += "\"emailTLS\":" + String(config->emailTLS ? "true" : "false") + ",";
    json += "\"tempHigh\":" + String(config->tempHighThreshold, 1) + ",";
    json += "\"tempLow\":" + String(config->tempLowThreshold, 1) + ",";
    json += "\"batteryLow\":" + String(config->batteryLowThreshold) + ",";
    json += "\"batteryCritical\":" + String(config->batteryCriticalThreshold) + ",";
    json += "\"timeout\":" + String(config->sensorTimeoutMinutes) + ",";
    json += "\"rateLimit\":" + String(config->rateLimitSeconds) + ",";
    json += "\"alertTempHigh\":" + String(config->alertTempHigh ? "true" : "false") + ",";
    json += "\"alertTempLow\":" + String(config->alertTempLow ? "true" : "false") + ",";
    json += "\"alertBatteryLow\":" + String(config->alertBatteryLow ? "true" : "false") + ",";
    json += "\"alertBatteryCritical\":" + String(config->alertBatteryCritical ? "true" : "false") + ",";
    json += "\"alertSensorOffline\":" + String(config->alertSensorOffline ? "true" : "false") + ",";
    json += "\"alertSensorOnline\":" + String(config->alertSensorOnline ? "true" : "false");
    json += "}";
    
    return json;
}

void WiFiPortal::handleAlertsConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    Serial.println("Updating alerts config: " + body);
    
    // Parse JSON manually (simple parsing for known structure)
    AlertConfig* config = alertManager.getConfig();
    
    // Extract webhook URL
    int webhookStart = body.indexOf("\"webhook\":\"") + 11;
    int webhookEnd = body.indexOf("\"", webhookStart);
    if (webhookStart > 10 && webhookEnd > webhookStart) {
        String webhook = body.substring(webhookStart, webhookEnd);
        alertManager.setTeamsWebhook(webhook.c_str());
    }
    
    // Extract enabled status
    config->teamsEnabled = body.indexOf("\"enabled\":true") > 0;
    
    // Extract email configuration
    config->emailEnabled = body.indexOf("\"emailEnabled\":true") > 0;
    
    int smtpServerStart = body.indexOf("\"smtpServer\":\"") + 14;
    int smtpServerEnd = body.indexOf("\"", smtpServerStart);
    if (smtpServerStart > 13 && smtpServerEnd > smtpServerStart) {
        String server = body.substring(smtpServerStart, smtpServerEnd);
        strncpy(config->smtpServer, server.c_str(), sizeof(config->smtpServer) - 1);
    }
    
    int smtpPortIdx = body.indexOf("\"smtpPort\":") + 11;
    if (smtpPortIdx > 10) {
        config->smtpPort = body.substring(smtpPortIdx, body.indexOf(",", smtpPortIdx)).toInt();
    }
    
    int emailUserStart = body.indexOf("\"emailUser\":\"") + 13;
    int emailUserEnd = body.indexOf("\"", emailUserStart);
    if (emailUserStart > 12 && emailUserEnd > emailUserStart) {
        String user = body.substring(emailUserStart, emailUserEnd);
        strncpy(config->emailUser, user.c_str(), sizeof(config->emailUser) - 1);
    }
    
    int emailPassStart = body.indexOf("\"emailPassword\":\"") + 17;
    int emailPassEnd = body.indexOf("\"", emailPassStart);
    if (emailPassStart > 16 && emailPassEnd > emailPassStart) {
        String pass = body.substring(emailPassStart, emailPassEnd);
        strncpy(config->emailPassword, pass.c_str(), sizeof(config->emailPassword) - 1);
    }
    
    int emailFromStart = body.indexOf("\"emailFrom\":\"") + 13;
    int emailFromEnd = body.indexOf("\"", emailFromStart);
    if (emailFromStart > 12 && emailFromEnd > emailFromStart) {
        String from = body.substring(emailFromStart, emailFromEnd);
        strncpy(config->emailFrom, from.c_str(), sizeof(config->emailFrom) - 1);
    }
    
    int emailToStart = body.indexOf("\"emailTo\":\"") + 11;
    int emailToEnd = body.indexOf("\"", emailToStart);
    if (emailToStart > 10 && emailToEnd > emailToStart) {
        String to = body.substring(emailToStart, emailToEnd);
        strncpy(config->emailTo, to.c_str(), sizeof(config->emailTo) - 1);
    }
    
    config->emailTLS = body.indexOf("\"emailTLS\":true") > 0;
    
    // Extract temperature thresholds
    int tempHighIdx = body.indexOf("\"tempHigh\":") + 11;
    if (tempHighIdx > 10) {
        config->tempHighThreshold = body.substring(tempHighIdx, body.indexOf(",", tempHighIdx)).toFloat();
    }
    
    int tempLowIdx = body.indexOf("\"tempLow\":") + 10;
    if (tempLowIdx > 9) {
        config->tempLowThreshold = body.substring(tempLowIdx, body.indexOf(",", tempLowIdx)).toFloat();
    }
    
    // Extract battery thresholds
    int battLowIdx = body.indexOf("\"batteryLow\":") + 13;
    if (battLowIdx > 12) {
        config->batteryLowThreshold = body.substring(battLowIdx, body.indexOf(",", battLowIdx)).toInt();
    }
    
    int battCritIdx = body.indexOf("\"batteryCritical\":") + 18;
    if (battCritIdx > 17) {
        config->batteryCriticalThreshold = body.substring(battCritIdx, body.indexOf(",", battCritIdx)).toInt();
    }
    
    // Extract sensor timeout
    int timeoutIdx = body.indexOf("\"timeout\":") + 10;
    if (timeoutIdx > 9) {
        config->sensorTimeoutMinutes = body.substring(timeoutIdx, body.indexOf(",", timeoutIdx)).toInt();
    }
    
    // Extract alert enable flags
    config->alertTempHigh = body.indexOf("\"alertTempHigh\":true") > 0;
    config->alertTempLow = body.indexOf("\"alertTempLow\":true") > 0;
    config->alertBatteryLow = body.indexOf("\"alertBatteryLow\":true") > 0;
    config->alertBatteryCritical = body.indexOf("\"alertBatteryCritical\":true") > 0;
    config->alertSensorOffline = body.indexOf("\"alertSensorOffline\":true") > 0;
    config->alertSensorOnline = body.indexOf("\"alertSensorOnline\":true") > 0;
    
    // Save configuration
    alertManager.saveConfig();
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
}

String WiFiPortal::generateAlertsPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Alert Configuration - LoRa Sensor Station</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
            background: white;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            padding: 30px;
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .nav-links {
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 2px solid #e0e0e0;
        }
        .nav-links a {
            display: inline-block;
            padding: 8px 16px;
            margin-right: 10px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            transition: background 0.3s;
        }
        .nav-links a:hover {
            background: #764ba2;
        }
        .section {
            margin-bottom: 30px;
            padding: 20px;
            background: #f9f9f9;
            border-radius: 8px;
        }
        .section h2 {
            color: #333;
            margin-bottom: 15px;
            font-size: 20px;
            display: flex;
            align-items: center;
        }
        .section h2::before {
            content: '';
            display: inline-block;
            width: 4px;
            height: 20px;
            background: #667eea;
            margin-right: 10px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 500;
            font-size: 14px;
        }
        input[type="text"],
        input[type="number"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input[type="text"]:focus,
        input[type="number"]:focus {
            outline: none;
            border-color: #667eea;
        }
        .checkbox-group {
            display: flex;
            align-items: center;
            padding: 10px;
            background: white;
            border-radius: 6px;
            margin-bottom: 10px;
        }
        input[type="checkbox"] {
            width: 20px;
            height: 20px;
            margin-right: 12px;
            cursor: pointer;
        }
        .checkbox-group label {
            margin: 0;
            cursor: pointer;
            flex: 1;
        }
        .threshold-row {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        .btn {
            padding: 14px 28px;
            border: none;
            border-radius: 6px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            margin-right: 10px;
        }
        .btn-primary {
            background: #667eea;
            color: white;
        }
        .btn-primary:hover {
            background: #764ba2;
        }
        .btn-secondary {
            background: #6c757d;
            color: white;
        }
        .btn-secondary:hover {
            background: #5a6268;
        }
        .btn-test {
            background: #28a745;
            color: white;
        }
        .btn-test:hover {
            background: #218838;
        }
        .alert {
            padding: 15px;
            border-radius: 6px;
            margin-bottom: 20px;
            font-size: 14px;
        }
        .alert-success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .alert-error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .alert-info {
            background: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }
        .hidden {
            display: none;
        }
        .help-text {
            font-size: 12px;
            color: #666;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔔 Alert Configuration</h1>
        <p class="subtitle">Configure Microsoft Teams notifications and alert thresholds</p>
        
        <div class="nav-links">
            <a href="/">← Back to Dashboard</a>
        </div>
        
        <div id="messageBox" class="alert hidden"></div>
        
        <form id="alertsForm">
            <!-- Teams Webhook -->
            <div class="section">
                <h2>Microsoft Teams Integration</h2>
                <div class="form-group">
                    <div class="checkbox-group">
                        <input type="checkbox" id="teamsEnabled" name="teamsEnabled">
                        <label for="teamsEnabled">Enable Teams Notifications</label>
                    </div>
                </div>
                <div class="form-group">
                    <label for="teamsWebhook">Teams Webhook URL</label>
                    <input type="text" id="teamsWebhook" name="teamsWebhook" placeholder="https://outlook.office.com/webhook/...">
                    <div class="help-text">Create an Incoming Webhook connector in your Teams channel</div>
                </div>
                <button type="button" class="btn btn-test" onclick="testWebhook()">🧪 Send Test Alert</button>
            </div>
            
            <!-- Email Configuration -->
            <div class="section">
                <h2>Email Notifications</h2>
                <div class="form-group">
                    <div class="checkbox-group">
                        <input type="checkbox" id="emailEnabled" name="emailEnabled">
                        <label for="emailEnabled">Enable Email Notifications</label>
                    </div>
                </div>
                <div class="threshold-row">
                    <div class="form-group">
                        <label for="smtpServer">SMTP Server</label>
                        <input type="text" id="smtpServer" name="smtpServer" placeholder="smtp.gmail.com">
                    </div>
                    <div class="form-group">
                        <label for="smtpPort">SMTP Port</label>
                        <input type="number" id="smtpPort" name="smtpPort" value="587">
                        <div class="help-text">Common: 587 (TLS), 465 (SSL), 25 (unsecured)</div>
                    </div>
                </div>
                <div class="form-group">
                    <label for="emailUser">Email Username</label>
                    <input type="text" id="emailUser" name="emailUser" placeholder="your-email@gmail.com">
                </div>
                <div class="form-group">
                    <label for="emailPassword">Email Password</label>
                    <input type="password" id="emailPassword" name="emailPassword" placeholder="App Password">
                    <div class="help-text">For Gmail: Use an App Password (not your regular password)</div>
                </div>
                <div class="form-group">
                    <label for="emailFrom">From Email</label>
                    <input type="text" id="emailFrom" name="emailFrom" placeholder="alerts@yourdomain.com">
                </div>
                <div class="form-group">
                    <label for="emailTo">To Email</label>
                    <input type="text" id="emailTo" name="emailTo" placeholder="recipient@example.com">
                    <div class="help-text">Comma-separated for multiple recipients</div>
                </div>
                <div class="form-group">
                    <div class="checkbox-group">
                        <input type="checkbox" id="emailTLS" name="emailTLS" checked>
                        <label for="emailTLS">Use TLS/STARTTLS</label>
                    </div>
                </div>
                <button type="button" class="btn btn-test" onclick="testEmail()">📧 Send Test Email</button>
            </div>
            
            <!-- Temperature Thresholds -->
            <div class="section">
                <h2>Temperature Thresholds</h2>
                <div class="threshold-row">
                    <div class="form-group">
                        <label for="tempLow">Low Temperature (°C)</label>
                        <input type="number" id="tempLow" name="tempLow" step="0.1" value="10.0">
                    </div>
                    <div class="form-group">
                        <label for="tempHigh">High Temperature (°C)</label>
                        <input type="number" id="tempHigh" name="tempHigh" step="0.1" value="30.0">
                    </div>
                </div>
                <div class="checkbox-group">
                    <input type="checkbox" id="alertTempHigh" name="alertTempHigh" checked>
                    <label for="alertTempHigh">Alert on high temperature</label>
                </div>
                <div class="checkbox-group">
                    <input type="checkbox" id="alertTempLow" name="alertTempLow" checked>
                    <label for="alertTempLow">Alert on low temperature</label>
                </div>
            </div>
            
            <!-- Battery Thresholds -->
            <div class="section">
                <h2>Battery Thresholds</h2>
                <div class="threshold-row">
                    <div class="form-group">
                        <label for="batteryLow">Low Battery (%)</label>
                        <input type="number" id="batteryLow" name="batteryLow" min="0" max="100" value="20">
                    </div>
                    <div class="form-group">
                        <label for="batteryCritical">Critical Battery (%)</label>
                        <input type="number" id="batteryCritical" name="batteryCritical" min="0" max="100" value="10">
                    </div>
                </div>
                <div class="checkbox-group">
                    <input type="checkbox" id="alertBatteryLow" name="alertBatteryLow" checked>
                    <label for="alertBatteryLow">Alert on low battery</label>
                </div>
                <div class="checkbox-group">
                    <input type="checkbox" id="alertBatteryCritical" name="alertBatteryCritical" checked>
                    <label for="alertBatteryCritical">Alert on critical battery</label>
                </div>
            </div>
            
            <!-- Sensor Status -->
            <div class="section">
                <h2>Client Status Alerts</h2>
                <div class="form-group">
                    <label for="sensorTimeout">Client Inactivity Timeout (minutes)</label>
                    <input type="number" id="sensorTimeout" name="sensorTimeout" min="1" max="1440" value="15">
                    <div class="help-text">Alert when a client hasn't been seen for this many minutes (default: 15 minutes)</div>
                </div>
                <div class="checkbox-group">
                    <input type="checkbox" id="alertSensorOffline" name="alertSensorOffline" checked>
                    <label for="alertSensorOffline">Alert when client goes offline (exceeds inactivity timeout)</label>
                </div>
                <div class="checkbox-group">
                    <input type="checkbox" id="alertSensorOnline" name="alertSensorOnline">
                    <label for="alertSensorOnline">Alert when client comes back online</label>
                </div>
            </div>
            
            <button type="submit" class="btn btn-primary">💾 Save Configuration</button>
            <button type="button" class="btn btn-secondary" onclick="location.href='/'">Cancel</button>
        </form>
    </div>
    
    <script>
        // Load current configuration
        async function loadConfig() {
            try {
                const response = await fetch('/api/alerts/config');
                const config = await response.json();
                
                document.getElementById('teamsEnabled').checked = config.teamsEnabled;
                document.getElementById('teamsWebhook').value = config.teamsWebhook;
                document.getElementById('emailEnabled').checked = config.emailEnabled;
                document.getElementById('smtpServer').value = config.smtpServer;
                document.getElementById('smtpPort').value = config.smtpPort;
                document.getElementById('emailUser').value = config.emailUser;
                document.getElementById('emailPassword').value = config.emailPassword;
                document.getElementById('emailFrom').value = config.emailFrom;
                document.getElementById('emailTo').value = config.emailTo;
                document.getElementById('emailTLS').checked = config.emailTLS;
                document.getElementById('tempHigh').value = config.tempHigh;
                document.getElementById('tempLow').value = config.tempLow;
                document.getElementById('batteryLow').value = config.batteryLow;
                document.getElementById('batteryCritical').value = config.batteryCritical;
                document.getElementById('sensorTimeout').value = config.timeout;
                document.getElementById('alertTempHigh').checked = config.alertTempHigh;
                document.getElementById('alertTempLow').checked = config.alertTempLow;
                document.getElementById('alertBatteryLow').checked = config.alertBatteryLow;
                document.getElementById('alertBatteryCritical').checked = config.alertBatteryCritical;
                document.getElementById('alertSensorOffline').checked = config.alertSensorOffline;
                document.getElementById('alertSensorOnline').checked = config.alertSensorOnline;
            } catch (error) {
                showMessage('Failed to load configuration', 'error');
            }
        }
        
        // Save configuration
        document.getElementById('alertsForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const config = {
                enabled: document.getElementById('teamsEnabled').checked,
                webhook: document.getElementById('teamsWebhook').value,
                emailEnabled: document.getElementById('emailEnabled').checked,
                smtpServer: document.getElementById('smtpServer').value,
                smtpPort: parseInt(document.getElementById('smtpPort').value),
                emailUser: document.getElementById('emailUser').value,
                emailPassword: document.getElementById('emailPassword').value,
                emailFrom: document.getElementById('emailFrom').value,
                emailTo: document.getElementById('emailTo').value,
                emailTLS: document.getElementById('emailTLS').checked,
                tempHigh: parseFloat(document.getElementById('tempHigh').value),
                tempLow: parseFloat(document.getElementById('tempLow').value),
                batteryLow: parseInt(document.getElementById('batteryLow').value),
                batteryCritical: parseInt(document.getElementById('batteryCritical').value),
                timeout: parseInt(document.getElementById('sensorTimeout').value),
                alertTempHigh: document.getElementById('alertTempHigh').checked,
                alertTempLow: document.getElementById('alertTempLow').checked,
                alertBatteryLow: document.getElementById('alertBatteryLow').checked,
                alertBatteryCritical: document.getElementById('alertBatteryCritical').checked,
                alertSensorOffline: document.getElementById('alertSensorOffline').checked,
                alertSensorOnline: document.getElementById('alertSensorOnline').checked
            };
            
            try {
                const response = await fetch('/api/alerts/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(config)
                });
                
                const result = await response.json();
                if (result.success) {
                    showMessage('✅ Configuration saved successfully!', 'success');
                } else {
                    showMessage('❌ Failed to save configuration', 'error');
                }
            } catch (error) {
                showMessage('❌ Error saving configuration: ' + error.message, 'error');
            }
        });
        
        // Test webhook
        async function testWebhook() {
            const webhook = document.getElementById('teamsWebhook').value;
            if (!webhook) {
                showMessage('⚠️ Please enter a webhook URL first', 'error');
                return;
            }
            
            showMessage('🔄 Sending test alert...', 'info');
            
            try {
                const response = await fetch('/api/alerts/test', { method: 'POST' });
                const result = await response.json();
                
                if (result.success) {
                    showMessage('✅ Test alert sent! Check your Teams channel.', 'success');
                } else {
                    showMessage('❌ Failed to send test alert: ' + result.message, 'error');
                }
            } catch (error) {
                showMessage('❌ Error sending test alert: ' + error.message, 'error');
            }
        }
        
        // Test email
        async function testEmail() {
            const emailTo = document.getElementById('emailTo').value;
            if (!emailTo) {
                showMessage('⚠️ Please enter a recipient email address first', 'error');
                return;
            }
            
            showMessage('🔄 Sending test email...', 'info');
            
            try {
                const response = await fetch('/api/alerts/test-email', { method: 'POST' });
                const result = await response.json();
                
                if (result.success) {
                    showMessage('✅ Test email sent! Check your inbox.', 'success');
                } else {
                    showMessage('❌ Failed to send test email: ' + result.message, 'error');
                }
            } catch (error) {
                showMessage('❌ Error sending test email: ' + error.message, 'error');
            }
        }
        
        // Show message
        function showMessage(message, type) {
            const box = document.getElementById('messageBox');
            box.textContent = message;
            box.className = 'alert alert-' + type;
            box.classList.remove('hidden');
            
            if (type === 'success' || type === 'info') {
                setTimeout(() => box.classList.add('hidden'), 5000);
            }
        }
        
        // Load config on page load
        loadConfig();
    </script>
</body>
</html>
)rawliteral";
    return html;
}

bool WiFiPortal::testTeamsWebhook() {
    return alertManager.testTeamsWebhook();
}

#ifdef BASE_STATION
/**
 * @brief Generate MQTT configuration JSON
 */
String WiFiPortal::generateMQTTConfigJSON() {
    MQTTConfig* config = mqttClient.getConfig();
    
    String json = "{";
    json += "\"enabled\":" + String(config->enabled ? "true" : "false") + ",";
    json += "\"broker\":\"" + String(config->broker) + "\",";
    json += "\"port\":" + String(config->port) + ",";
    json += "\"username\":\"" + String(config->username) + "\",";
    json += "\"password\":\"" + String(config->password) + "\",";
    json += "\"topicPrefix\":\"" + String(config->topicPrefix) + "\",";
    json += "\"haDiscovery\":" + String(config->homeAssistantDiscovery ? "true" : "false") + ",";
    json += "\"qos\":" + String(config->qos);
    json += "}";
    
    return json;
}
#endif // BASE_STATION

/**
 * @brief Handle MQTT configuration update
 */
#ifdef BASE_STATION
void WiFiPortal::handleMQTTConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    Serial.println("Updating MQTT config: " + body);
    
    MQTTConfig* config = mqttClient.getConfig();
    
    // Parse enabled
    int enabledStart = body.indexOf("\"enabled\":") + 10;
    config->enabled = (body.substring(enabledStart, enabledStart + 4) == "true");
    
    // Parse broker
    int brokerStart = body.indexOf("\"broker\":\"") + 10;
    int brokerEnd = body.indexOf("\"", brokerStart);
    if (brokerStart > 9 && brokerEnd > brokerStart) {
        String broker = body.substring(brokerStart, brokerEnd);
        strncpy(config->broker, broker.c_str(), sizeof(config->broker) - 1);
    }
    
    // Parse port
    int portStart = body.indexOf("\"port\":") + 7;
    config->port = body.substring(portStart).toInt();
    
    // Parse username
    int usernameStart = body.indexOf("\"username\":\"") + 12;
    int usernameEnd = body.indexOf("\"", usernameStart);
    if (usernameStart > 11 && usernameEnd > usernameStart) {
        String username = body.substring(usernameStart, usernameEnd);
        strncpy(config->username, username.c_str(), sizeof(config->username) - 1);
    }
    
    // Parse password
    int passwordStart = body.indexOf("\"password\":\"") + 12;
    int passwordEnd = body.indexOf("\"", passwordStart);
    if (passwordStart > 11 && passwordEnd > passwordStart) {
        String password = body.substring(passwordStart, passwordEnd);
        strncpy(config->password, password.c_str(), sizeof(config->password) - 1);
    }
    
    // Parse topic prefix
    int prefixStart = body.indexOf("\"topicPrefix\":\"") + 15;
    int prefixEnd = body.indexOf("\"", prefixStart);
    if (prefixStart > 14 && prefixEnd > prefixStart) {
        String prefix = body.substring(prefixStart, prefixEnd);
        strncpy(config->topicPrefix, prefix.c_str(), sizeof(config->topicPrefix) - 1);
    }
    
    // Parse Home Assistant discovery
    int haStart = body.indexOf("\"haDiscovery\":") + 14;
    config->homeAssistantDiscovery = (body.substring(haStart, haStart + 4) == "true");
    
    // Parse QoS
    int qosStart = body.indexOf("\"qos\":") + 6;
    config->qos = body.substring(qosStart).toInt();
    
    // Save configuration
    mqttClient.saveConfig();
    
    // Restart MQTT client
    mqttClient.disconnect();
    mqttClient.begin();
    
    String response = "{\"success\":true,\"message\":\"MQTT configuration saved\"}";
    request->send(200, "application/json", response);
}
#endif // BASE_STATION

/**
 * @brief Generate MQTT configuration page
 */
#ifdef BASE_STATION
String WiFiPortal::generateMQTTPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>MQTT Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f7fa; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 15px; margin-bottom: 20px; }
        .header h1 { font-size: 28px; margin-bottom: 5px; }
        .card { background: white; border-radius: 12px; padding: 25px; margin-bottom: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 20px; }
        label { display: block; font-weight: 600; margin-bottom: 8px; color: #333; }
        input[type="text"], input[type="number"], input[type="password"] { width: 100%; padding: 10px; border: 2px solid #e0e0e0; border-radius: 6px; font-size: 14px; }
        input[type="checkbox"] { width: 20px; height: 20px; margin-right: 8px; }
        .checkbox-label { display: flex; align-items: center; margin-bottom: 10px; }
        .btn { padding: 12px 24px; border: none; border-radius: 6px; font-size: 14px; font-weight: 600; cursor: pointer; transition: all 0.3s; }
        .btn-primary { background: #667eea; color: white; }
        .btn-primary:hover { background: #5568d3; }
        .btn-secondary { background: #6b7280; color: white; margin-left: 10px; }
        .btn-secondary:hover { background: #4b5563; }
        .alert { padding: 15px; border-radius: 6px; margin-bottom: 20px; }
        .alert-success { background: #d1fae5; color: #065f46; }
        .alert-error { background: #fee2e2; color: #991b1b; }
        .alert-info { background: #dbeafe; color: #1e40af; }
        .hidden { display: none; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-top: 20px; }
        .stat-box { background: #f9f9f9; padding: 15px; border-radius: 8px; text-align: center; }
        .stat-label { font-size: 12px; color: #666; text-transform: uppercase; margin-bottom: 5px; }
        .stat-value { font-size: 24px; font-weight: bold; color: #333; }
        .back-link { display: inline-block; color: white; text-decoration: none; margin-top: 10px; opacity: 0.9; }
        .back-link:hover { opacity: 1; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1> MQTT Configuration</h1>
            <p>Connect to MQTT broker for data publishing</p>
            <a href="/" class="back-link"> Back to Dashboard</a>
        </div>
        
        <div id="messageBox" class="alert hidden"></div>
        
        <div class="card">
            <h2 style="margin-bottom: 20px;">MQTT Connection</h2>
            <form id="mqttForm">
                <div class="form-group">
                    <div class="checkbox-label">
                        <input type="checkbox" id="enabled">
                        <label for="enabled">Enable MQTT Publishing</label>
                    </div>
                </div>
                
                <div class="form-group">
                    <label for="broker">MQTT Broker:</label>
                    <input type="text" id="broker" placeholder="192.168.1.100 or mqtt.example.com">
                </div>
                
                <div class="form-group">
                    <label for="port">Port:</label>
                    <input type="number" id="port" value="1883">
                </div>
                
                <div class="form-group">
                    <label for="username">Username (optional):</label>
                    <input type="text" id="username" placeholder="mqtt_user">
                </div>
                
                <div class="form-group">
                    <label for="password">Password (optional):</label>
                    <input type="password" id="password" placeholder="mqtt_password">
                </div>
                
                <div class="form-group">
                    <label for="topicPrefix">Topic Prefix:</label>
                    <input type="text" id="topicPrefix" value="lora" placeholder="lora">
                    <small style="color: #666;">Topics: {prefix}/sensor/{id}/temperature</small>
                </div>
                
                <div class="form-group">
                    <div class="checkbox-label">
                        <input type="checkbox" id="haDiscovery" checked>
                        <label for="haDiscovery">Enable Home Assistant Auto-Discovery</label>
                    </div>
                </div>
                
                <div class="form-group">
                    <label for="qos">QoS Level:</label>
                    <input type="number" id="qos" value="0" min="0" max="2">
                    <small style="color: #666;">0 = At most once, 1 = At least once, 2 = Exactly once</small>
                </div>
                
                <div style="margin-top: 30px;">
                    <button type="submit" class="btn btn-primary"> Save Configuration</button>
                    <button type="button" class="btn btn-secondary" onclick="testConnection()"> Test Connection</button>
                </div>
            </form>
        </div>
        
        <div class="card">
            <h2 style="margin-bottom: 20px;">MQTT Statistics</h2>
            <div class="stats">
                <div class="stat-box">
                    <div class="stat-label">Status</div>
                    <div class="stat-value" id="mqttStatus">-</div>
                </div>
                <div class="stat-box">
                    <div class="stat-label">Published</div>
                    <div class="stat-value" id="mqttPublishes">0</div>
                </div>
                <div class="stat-box">
                    <div class="stat-label">Failed</div>
                    <div class="stat-value" id="mqttFailures">0</div>
                </div>
                <div class="stat-box">
                    <div class="stat-label">Reconnects</div>
                    <div class="stat-value" id="mqttReconnects">0</div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        async function loadConfig() {
            try {
                const response = await fetch('/api/mqtt/config');
                const config = await response.json();
                
                document.getElementById('enabled').checked = config.enabled;
                document.getElementById('broker').value = config.broker;
                document.getElementById('port').value = config.port;
                document.getElementById('username').value = config.username;
                document.getElementById('password').value = config.password;
                document.getElementById('topicPrefix').value = config.topicPrefix;
                document.getElementById('haDiscovery').checked = config.haDiscovery;
                document.getElementById('qos').value = config.qos;
            } catch (error) {
                showMessage('Failed to load configuration', 'error');
            }
        }
        
        async function loadStats() {
            try {
                const response = await fetch('/api/mqtt/stats');
                const stats = await response.json();
                
                document.getElementById('mqttStatus').textContent = stats.connected ? ' Connected' : ' Disconnected';
                document.getElementById('mqttPublishes').textContent = stats.publishes;
                document.getElementById('mqttFailures').textContent = stats.failures;
                document.getElementById('mqttReconnects').textContent = stats.reconnects;
            } catch (error) {
                console.error('Failed to load stats:', error);
            }
        }
        
        document.getElementById('mqttForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const config = {
                enabled: document.getElementById('enabled').checked,
                broker: document.getElementById('broker').value,
                port: parseInt(document.getElementById('port').value),
                username: document.getElementById('username').value,
                password: document.getElementById('password').value,
                topicPrefix: document.getElementById('topicPrefix').value,
                haDiscovery: document.getElementById('haDiscovery').checked,
                qos: parseInt(document.getElementById('qos').value)
            };
            
            try {
                const response = await fetch('/api/mqtt/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(config)
                });
                
                const result = await response.json();
                if (result.success) {
                    showMessage(' Configuration saved successfully!', 'success');
                    setTimeout(loadStats, 1000);
                } else {
                    showMessage(' Failed to save configuration', 'error');
                }
            } catch (error) {
                showMessage(' Error saving configuration: ' + error.message, 'error');
            }
        });
        
        async function testConnection() {
            showMessage(' Testing MQTT connection...', 'info');
            
            try {
                const response = await fetch('/api/mqtt/test', { method: 'POST' });
                const result = await response.json();
                
                if (result.success) {
                    showMessage(' MQTT connection successful!', 'success');
                    setTimeout(loadStats, 500);
                } else {
                    showMessage(' Connection failed: ' + result.message, 'error');
                }
            } catch (error) {
                showMessage(' Error testing connection: ' + error.message, 'error');
            }
        }
        
        function showMessage(message, type) {
            const box = document.getElementById('messageBox');
            box.textContent = message;
            box.className = 'alert alert-' + type;
            box.classList.remove('hidden');
            
            if (type === 'success' || type === 'info') {
                setTimeout(() => box.classList.add('hidden'), 5000);
            }
        }
        
        loadConfig();
        loadStats();
        setInterval(loadStats, 5000);
    </script>
</body>
</html>
)rawliteral";
    return html;
}

String WiFiPortal::generateSensorNamesJSON() {
    extern SensorConfigManager sensorConfigManager;
    
    String json = "[";
    SensorInfo* sensors = getAllSensors();
    int sensorCount = getActiveSensorCount();
    bool first = true;
    
    for (int i = 0; i < 10; i++) {
        if (sensors[i].active) {
            if (!first) json += ",";
            first = false;
            
            SensorMetadata meta = sensorConfigManager.getSensorMetadata(sensors[i].sensorId);
            
            json += "{";
            json += "\"id\":" + String(sensors[i].sensorId) + ",";
            json += "\"location\":\"" + String(meta.location) + "\",";
            json += "\"notes\":\"" + String(meta.notes) + "\"";
            json += "}";
        }
    }
    
    json += "]";
    return json;
}

void WiFiPortal::handleSensorNameUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    extern SensorConfigManager sensorConfigManager;
    
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    // Simple JSON parsing (format: {"id":1,"location":"Name","notes":"Notes"})
    int idStart = body.indexOf("\"id\":") + 5;
    int idEnd = body.indexOf(",", idStart);
    int sensorId = body.substring(idStart, idEnd).toInt();
    
    int locStart = body.indexOf("\"location\":\"") + 12;
    int locEnd = body.indexOf("\"", locStart);
    String location = body.substring(locStart, locEnd);
    
    int notesStart = body.indexOf("\"notes\":\"") + 9;
    int notesEnd = body.indexOf("\"", notesStart);
    String notes = body.substring(notesStart, notesEnd);
    
    // Save to config
    SensorMetadata meta;
    meta.sensorId = sensorId;
    strncpy(meta.location, location.c_str(), sizeof(meta.location) - 1);
    meta.location[sizeof(meta.location) - 1] = '\0';
    strncpy(meta.notes, notes.c_str(), sizeof(meta.notes) - 1);
    meta.notes[sizeof(meta.notes) - 1] = '\0';
    meta.transmitInterval = 15;
    meta.tempThresholdMin = -40.0;
    meta.tempThresholdMax = 85.0;
    meta.alertsEnabled = true;
    meta.configured = true;
    
    bool success = sensorConfigManager.setSensorMetadata(sensorId, meta);
    
    String response = success ? 
        "{\"success\":true,\"message\":\"Sensor name saved!\"}" : 
        "{\"success\":false,\"message\":\"Failed to save sensor name\"}";
    request->send(success ? 200 : 500, "application/json", response);
}

void WiFiPortal::handleRemoteSetInterval(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    extern RemoteConfigManager remoteConfigManager;
    
    Serial.println("=== handleRemoteSetInterval called ===");
    Serial.printf("Data ptr: %p, len: %d\n", data, len);
    
    // Build body string character by character (safer than memcpy)
    String body = "";
    body.reserve(len + 1);
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    Serial.printf("Received JSON: %s\n", body.c_str());
    
    // Simple JSON parsing - handle both {"id":1} and {id:1} formats
    int idStart = body.indexOf("id");
    int intervalStart = body.indexOf("interval");
    
    if (idStart < 0 || intervalStart < 0) {
        Serial.println("ERROR: Invalid JSON format - missing id or interval");
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Extract values - find the colon after the key
    idStart = body.indexOf(":", idStart) + 1;
    int idEnd = body.indexOf(",", idStart);
    if (idEnd < 0) idEnd = body.indexOf("}", idStart);
    uint8_t sensorId = body.substring(idStart, idEnd).toInt();
    
    intervalStart = body.indexOf(":", intervalStart) + 1;
    int intervalEnd = body.indexOf("}", intervalStart);
    if (intervalEnd < 0) intervalEnd = body.indexOf(",", intervalStart);
    uint16_t interval = body.substring(intervalStart, intervalEnd).toInt();
    
    Serial.printf("Remote config: Set interval for sensor %d to %d seconds\n", sensorId, interval);
    
    // Pack interval data as little-endian
    uint8_t intervalData[2];
    intervalData[0] = interval & 0xFF;
    intervalData[1] = (interval >> 8) & 0xFF;
    
    Serial.printf("DEBUG: Packing interval %d as bytes: 0x%02X 0x%02X\n", interval, intervalData[0], intervalData[1]);
    
    // Queue command
    bool success = remoteConfigManager.queueCommand(sensorId, CMD_SET_INTERVAL, intervalData, 2);
    
    String response = success ? 
        "{\"success\":true,\"message\":\"Interval command queued\"}" : 
        "{\"success\":false,\"message\":\"Failed to queue command\"}";
    request->send(success ? 200 : 500, "application/json", response);
}

void WiFiPortal::handleRemoteRestart(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    extern RemoteConfigManager remoteConfigManager;
    
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    // Parse JSON: {"id":1}
    int idStart = body.indexOf("\"id\":") + 5;
    int idEnd = body.indexOf("}", idStart);
    uint8_t sensorId = body.substring(idStart, idEnd).toInt();
    
    // Queue restart command
    bool success = remoteConfigManager.queueCommand(sensorId, CMD_RESTART, nullptr, 0);
    
    String response = success ? 
        "{\"success\":true,\"message\":\"Restart command queued\"}" : 
        "{\"success\":false,\"message\":\"Failed to queue command\"}";
    request->send(success ? 200 : 500, "application/json", response);
}

void WiFiPortal::handleRemoteGetConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    extern RemoteConfigManager remoteConfigManager;
    
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    // Parse JSON: {"id":1}
    int idStart = body.indexOf("\"id\":") + 5;
    int idEnd = body.indexOf("}", idStart);
    uint8_t sensorId = body.substring(idStart, idEnd).toInt();
    
    // Queue get config command
    bool success = remoteConfigManager.queueCommand(sensorId, CMD_GET_CONFIG, nullptr, 0);
    
    String response = success ? 
        "{\"success\":true,\"message\":\"Get config command queued\"}" : 
        "{\"success\":false,\"message\":\"Failed to queue command\"}";
    request->send(success ? 200 : 500, "application/json", response);
}

String WiFiPortal::generateCommandQueueJSON() {
    extern RemoteConfigManager remoteConfigManager;
    
    String json = "[";
    bool first = true;
    
    // Check queue status for all active sensors
    for (int i = 0; i < 10; i++) {
        SensorInfo* sensor = getSensorByIndex(i);
        if (sensor != NULL) {
            uint8_t queuedCount = remoteConfigManager.getQueuedCount(sensor->sensorId);
            
            if (!first) json += ",";
            first = false;
            
            json += "{";
            json += "\"sensorId\":" + String(sensor->sensorId) + ",";
            json += "\"queuedCommands\":" + String(queuedCount);
            json += "}";
        }
    }
    
    json += "]";
    return json;
}

String WiFiPortal::generateSensorNamesPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Sensor Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: #f0f0f0;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            text-align: center;
            margin-bottom: 30px;
        }
        .card {
            background: white;
            padding: 25px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }
        .sensor-item {
            border-bottom: 1px solid #eee;
            padding: 15px 0;
        }
        .sensor-item:last-child {
            border-bottom: none;
        }
        .sensor-id {
            font-weight: bold;
            color: #667eea;
            font-size: 18px;
            margin-bottom: 10px;
        }
        label {
            display: block;
            margin-top: 10px;
            margin-bottom: 5px;
            font-weight: bold;
            color: #666;
        }
        input, textarea {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 14px;
        }
        textarea {
            resize: vertical;
            min-height: 60px;
        }
        .button {
            background: #667eea;
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            margin-top: 10px;
            font-size: 14px;
        }
        .button:hover {
            background: #5568d3;
        }
        .nav-buttons {
            text-align: center;
            margin-top: 20px;
        }
        .nav-buttons a {
            display: inline-block;
            margin: 0 10px;
            padding: 12px 25px;
            background: #764ba2;
            color: white;
            text-decoration: none;
            border-radius: 5px;
        }
        .alert {
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
        }
        .alert-success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .alert-error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .hidden {
            display: none;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🔧 Sensor Configuration</h1>
        <p>Configure sensor names and locations</p>
    </div>

    <div id="messageBox" class="alert hidden"></div>

    <div class="card">
        <h2>Active Sensors</h2>
        <div id="sensorList">
            <p>Loading sensors...</p>
        </div>
    </div>

    <div class="nav-buttons">
        <a href="/">← Dashboard</a>
        <a href="/alerts">Alerts</a>
        <a href="/mqtt">MQTT</a>
    </div>

    <script>
        async function loadSensors() {
            try {
                const response = await fetch('/api/sensor-names');
                const sensors = await response.json();
                
                const container = document.getElementById('sensorList');
                
                if (sensors.length === 0) {
                    container.innerHTML = '<p>No active sensors detected. Sensors will appear here once they transmit data.</p>';
                    return;
                }
                
                container.innerHTML = '';
                
                sensors.forEach(sensor => {
                    const div = document.createElement('div');
                    div.className = 'sensor-item';
                    div.innerHTML = `
                        <div class="sensor-id">Sensor ${sensor.id}</div>
                        <label>Location/Name:</label>
                        <input type="text" id="loc_${sensor.id}" value="${sensor.location}" placeholder="e.g., Living Room, Chemical Closet">
                        <label>Notes (optional):</label>
                        <textarea id="notes_${sensor.id}" placeholder="Additional information about this sensor">${sensor.notes}</textarea>
                        <button class="button" onclick="saveSensor(${sensor.id})">Save</button>
                    `;
                    container.appendChild(div);
                });
            } catch (error) {
                console.error('Error loading sensors:', error);
                document.getElementById('sensorList').innerHTML = '<p>Error loading sensors</p>';
            }
        }
        
        async function saveSensor(id) {
            const location = document.getElementById(`loc_${id}`).value;
            const notes = document.getElementById(`notes_${id}`).value;
            
            if (!location) {
                showMessage('Please enter a location name', 'error');
                return;
            }
            
            try {
                const response = await fetch('/api/sensor-names', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ id, location, notes })
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('✓ ' + result.message, 'success');
                    setTimeout(() => location.reload(), 1500);
                } else {
                    showMessage('✗ ' + result.message, 'error');
                }
            } catch (error) {
                showMessage('Error saving: ' + error.message, 'error');
            }
        }
        
        function showMessage(message, type) {
            const box = document.getElementById('messageBox');
            box.textContent = message;
            box.className = 'alert alert-' + type;
            box.classList.remove('hidden');
            
            if (type === 'success') {
                setTimeout(() => box.classList.add('hidden'), 3000);
            }
        }
        
        loadSensors();
        setInterval(loadSensors, 10000); // Refresh every 10 seconds
    </script>
</body>
</html>
)rawliteral";
    return html;
}
#endif // BASE_STATION
