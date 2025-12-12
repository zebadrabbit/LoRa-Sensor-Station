#include "wifi_portal.h"
#include "led_control.h"

// Global instance
WiFiPortal wifiPortal;

// DNS server port
const byte DNS_PORT = 53;

WiFiPortal::WiFiPortal() : webServer(80), portalActive(false) {
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
        <h1>📡 Sensor Configuration</h1>
        <form action="/sensor" method="POST">
            <div class="form-group">
                <label for="sensorId">Sensor ID (1-255)</label>
                <input type="number" id="sensorId" name="sensorId" min="1" max="255" required>
                <p class="help-text">Unique identifier for this sensor</p>
            </div>
            
            <div class="form-group">
                <label for="location">Location / Name</label>
                <input type="text" id="location" name="location" maxlength="31" placeholder="e.g., Living Room, Garage" required>
                <p class="help-text">Descriptive name for the sensor location</p>
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
        request->hasParam("interval", true)) {
        
        SensorConfig config;
        config.sensorId = request->getParam("sensorId", true)->value().toInt();
        strncpy(config.location, request->getParam("location", true)->value().c_str(), sizeof(config.location) - 1);
        config.location[sizeof(config.location) - 1] = '\0';
        config.transmitInterval = request->getParam("interval", true)->value().toInt();
        config.configured = true;
        
        // Save configuration
        configStorage.setSensorConfig(config);
        configStorage.setDeviceMode(MODE_SENSOR);
        
        Serial.println("Sensor configuration saved:");
        Serial.printf("  ID: %d\n", config.sensorId);
        Serial.printf("  Location: %s\n", config.location);
        Serial.printf("  Interval: %d seconds\n", config.transmitInterval);
        
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
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
        BaseStationConfig config;
        strncpy(config.ssid, request->getParam("ssid", true)->value().c_str(), sizeof(config.ssid) - 1);
        config.ssid[sizeof(config.ssid) - 1] = '\0';
        strncpy(config.password, request->getParam("password", true)->value().c_str(), sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
        config.configured = true;
        
        Serial.println("Testing WiFi connection...");
        Serial.printf("  SSID: %s\n", config.ssid);
        
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

String WiFiPortal::getAPIP() {
    return WiFi.softAPIP().toString();
}
