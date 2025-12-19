#include "wifi_portal.h"
#include "led_control.h"
#include "statistics.h"
#include "alerts.h"
#include "security.h"
#include "logger.h"
#ifdef BASE_STATION
#include "mqtt_client.h"
#include "sensor_config.h"
#include "remote_config.h"
#endif
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <map>

// Global instance
WiFiPortal wifiPortal;

// WebSocket for live updates
AsyncWebSocket ws("/ws");

#ifdef BASE_STATION
// LoRa settings reboot coordination tracking
struct LoRaRebootTracker {
    std::map<uint8_t, bool> sensorAcks;  // sensorId -> ack received
    uint32_t commandStartTime = 0;
    bool trackingActive = false;
    uint8_t totalSensors = 0;
};
static LoRaRebootTracker loraRebootTracker;
#endif

// DNS server port
const byte DNS_PORT = 53;

WiFiPortal::WiFiPortal() : webServer(80), portalActive(false), dashboardActive(false) {
}

void WiFiPortal::startPortal() {
    Serial.println("Starting WiFi Captive Portal...");
    
    // Disable task watchdog for WiFi operations (re-enabled after)
    disableCore0WDT();
    
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
    
    // Re-enable watchdog
    enableCore0WDT();
    
    portalActive = true;
}

// Forward declaration
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

void WiFiPortal::setupWebServer() {
    // Initialize LittleFS for serving static files
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: LittleFS Mount Failed!");
        Serial.println("You must upload filesystem with: pio run --target uploadfs");
    } else {
        Serial.println("LittleFS mounted successfully");
        
        // List files to verify they exist
        File root = LittleFS.open("/");
        if (root) {
            Serial.println("Files in LittleFS:");
            File file = root.openNextFile();
            while(file) {
                Serial.print("  ");
                Serial.println(file.name());
                file.close();
                file = root.openNextFile();
            }
            root.close();
        }
    }
    
    // Serve main page from LittleFS
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Request: /");
        request->send(LittleFS, "/setup.html", "text/html");
    });
    
    // Captive portal detection
    webServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Request: /generate_204");
        request->send(LittleFS, "/setup.html", "text/html");
    });
    webServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Request: /hotspot-detect.html");
        request->send(LittleFS, "/setup.html", "text/html");
    });
    
    // Mode selection
    webServer.on("/mode", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleModeSelection(request);
    });
    
    // Sensor configuration page
    webServer.on("/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Request: /sensor");
        request->send(LittleFS, "/sensor-setup.html", "text/html");
    });
    
    webServer.on("/sensor", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSensorConfig(request);
    });
    
    // Base station configuration page
    webServer.on("/base", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Request: /base");
        request->send(LittleFS, "/base-setup.html", "text/html");
    });
    
    webServer.on("/base", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleBaseStationConfig(request);
    });
    
    // Success page
    webServer.on("/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Request: /success.html");
        request->send(LittleFS, "/success.html", "text/html");
    });
    
    // Catch-all for captive portal
    webServer.onNotFound([](AsyncWebServerRequest *request) {
        Serial.print("Request (404): ");
        Serial.println(request->url());
        request->send(LittleFS, "/setup.html", "text/html");
    });
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
        
        // Parse zone (optional)
        if (request->hasParam("zone", true)) {
            strncpy(config.zone, request->getParam("zone", true)->value().c_str(), sizeof(config.zone) - 1);
            config.zone[sizeof(config.zone) - 1] = '\0';
        } else {
            config.zone[0] = '\0';  // Empty zone
        }
        
        // Parse priority (default to MEDIUM)
        if (request->hasParam("priority", true)) {
            config.priority = (SensorPriority)request->getParam("priority", true)->value().toInt();
        } else {
            config.priority = PRIORITY_MEDIUM;
        }
        
        // Parse client type (default to STANDARD)
        if (request->hasParam("clientType", true)) {
            config.clientType = (ClientType)request->getParam("clientType", true)->value().toInt();
        } else {
            config.clientType = CLIENT_STANDARD;
        }
        
        config.configured = true;
        
        // Handle encryption key if provided
        if (request->hasParam("encryptionKey", true)) {
            String keyStr = request->getParam("encryptionKey", true)->value();
            keyStr.trim();
            keyStr.toUpperCase();
            
            if (keyStr.length() == 32) {
                // Valid key provided - parse and set it
                uint8_t key[16];
                bool validHex = true;
                for (int i = 0; i < 16; i++) {
                    String byteStr = keyStr.substring(i*2, i*2+2);
                    char* endPtr;
                    long val = strtol(byteStr.c_str(), &endPtr, 16);
                    if (*endPtr != '\0' || val < 0 || val > 255) {
                        validHex = false;
                        break;
                    }
                    key[i] = (uint8_t)val;
                }
                
                if (validHex) {
                    securityManager.setKey(key);
                    securityManager.setEncryptionEnabled(true);
                    securityManager.saveConfig();
                    Serial.println("Encryption key configured and enabled");
                } else {
                    Serial.println("Invalid hex key format - encryption not enabled");
                }
            } else if (keyStr.length() > 0) {
                Serial.println("Invalid key length (must be 32 hex characters) - encryption not enabled");
            } else {
                // Empty key - disable encryption
                securityManager.setEncryptionEnabled(false);
                securityManager.saveConfig();
                Serial.println("No encryption key provided - encryption disabled");
            }
        }
        
        // Save configuration
        configStorage.setSensorConfig(config);
        configStorage.setDeviceMode(MODE_SENSOR);
        
        Serial.println("Sensor configuration saved:");
        Serial.printf("  ID: %d\n", config.sensorId);
        Serial.printf("  Location: %s\n", config.location);
        Serial.printf("  Interval: %d seconds\n", config.transmitInterval);
        Serial.printf("  Network ID: %d\n", config.networkId);
        
        // Send success page
        String message = "Sensor ID " + String(config.sensorId) + " configured. Device will reboot and start transmitting data.";
        request->redirect("/success.html?message=" + message);
        
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
        
        // Test connection with proper verification
        WiFi.persistent(false);
        WiFi.setSleep(false);
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.password);
        
        uint32_t startTime = millis();
        uint32_t timeout = 10000;  // 10 second timeout
        bool connected = false;
        
        Serial.print("⏳ Waiting for connection");
        while (millis() - startTime < timeout) {
            if (isNetworkUsable()) {
                connected = true;
                break;
            }
            if ((millis() - startTime) % 500 == 0) {
                Serial.print(".");
            }
            delay(100);
        }
        Serial.println();
        
        if (connected) {
            Serial.println("✅ WiFi connection successful and verified!");
            Serial.print("   IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.print("   Gateway: ");
            Serial.println(WiFi.gatewayIP());
            
            // Save configuration
            configStorage.setBaseStationConfig(config);
            configStorage.setDeviceMode(MODE_BASE_STATION);
            
            String message = "Successfully connected to " + String(config.ssid) + ". IP Address: " + WiFi.localIP().toString() + ". Device will reboot and start base station mode.";
            request->redirect("/success.html?message=" + message);
            
            // Schedule reboot
            delay(3000);
            ESP.restart();
        } else {
            Serial.println("WiFi connection failed!");
            
            // Return to AP mode
            WiFi.mode(WIFI_AP);
            String message = "Failed to connect to " + String(config.ssid) + ". Please check password and try again.";
            request->redirect("/success.html?message=" + message);
        }
    } else {
        request->send(400, "text/html", "Missing required fields");
    }
}

// Check if network is truly usable (not just "connected")
bool WiFiPortal::isNetworkUsable() {
    // Must be connected with valid status
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    // Must have valid IP and gateway
    if (WiFi.localIP() == INADDR_NONE || WiFi.localIP()[0] == 0) {
        Serial.println("⚠️  No valid IP address");
        return false;
    }
    
    if (WiFi.gatewayIP() == INADDR_NONE || WiFi.gatewayIP()[0] == 0) {
        Serial.println("⚠️  No valid gateway");
        return false;
    }
    
    Serial.printf("✓ IP: %s, Gateway: %s\n", 
                 WiFi.localIP().toString().c_str(), 
                 WiFi.gatewayIP().toString().c_str());
    
    // Verify we can actually reach the gateway (local network check)
    Serial.print("🔍 Testing gateway connectivity...");
    WiFiClient testClient;
    testClient.setTimeout(300);  // 300ms timeout
    
    // Try port 80 first (HTTP), then 443 (HTTPS)
    bool gatewayReachable = testClient.connect(WiFi.gatewayIP(), 80);
    if (!gatewayReachable) {
        gatewayReachable = testClient.connect(WiFi.gatewayIP(), 443);
    }
    testClient.stop();
    
    if (gatewayReachable) {
        Serial.println(" ✅ Gateway reachable!");
        return true;
    } else {
        Serial.println(" ❌ Gateway unreachable!");
        return false;
    }
}

// Reset WiFi stack without full ESP restart
void WiFiPortal::resetWiFiStack() {
    Serial.println("🔄 Resetting WiFi stack...");
    
    // Disconnect and clear all WiFi state
    WiFi.disconnect(true, true);  // disconnect, erase config
    delay(100);
    
    // Turn off WiFi completely
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    // Re-enable in station mode
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // Disable sleep for stability
    delay(50);
    
    Serial.println("✓ WiFi stack reset complete");
}

bool WiFiPortal::connectToWiFi(const char* ssid, const char* password) {
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    
    // Try up to 3 times with WiFi stack resets between attempts
    for (int retry = 0; retry < 3; retry++) {
        if (retry > 0) {
            Serial.printf("\n🔁 Retry attempt %d/3\n", retry + 1);
            resetWiFiStack();
        }
        
        // Configure WiFi (persistent=false to avoid flash wear)
        WiFi.persistent(false);
        WiFi.setSleep(false);
        WiFi.mode(WIFI_STA);
        
        WiFi.begin(ssid, password);
        
        // Wait for connection with GOT_IP verification
        uint32_t startTime = millis();
        uint32_t timeout = 10000;  // 10 second timeout
        
        Serial.print("⏳ Waiting for connection");
        while (millis() - startTime < timeout) {
            // Check if network is truly usable (GOT_IP + gateway reachable)
            if (isNetworkUsable()) {
                Serial.println("\n✅ WiFi Connected and Verified!");
                Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
                Serial.printf("   DNS: %s\n", WiFi.dnsIP().toString().c_str());
                Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
                
                setLEDColor(0, 255, 0);  // Green for connected
                return true;
            }
            
            // Print progress dot every 500ms
            if ((millis() - startTime) % 500 == 0) {
                Serial.print(".");
            }
            
            delay(100);
        }
        
        Serial.println(" ⏱️  Timeout!");
    }
    
    // All retries failed
    Serial.println("\n❌ WiFi Connection Failed after 3 attempts!");
    Serial.println("💡 Tip: Check SSID/password, router settings, and signal strength");
    
    return false;
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
            Serial.printf("✅ WebSocket client #%u connected from %s (total clients: %d)\n", 
                         client->id(), client->remoteIP().toString().c_str(), server->count());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("❌ WebSocket client #%u disconnected (remaining: %d)\n", 
                         client->id(), server->count());
            break;
        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            Serial.printf("📨 WebSocket DATA from client #%u: %d bytes\n", client->id(), len);
            break;
        }
        case WS_EVT_PONG:
            Serial.printf("🏓 WebSocket PONG from client #%u\n", client->id());
            break;
        case WS_EVT_ERROR:
            Serial.printf("⚠️  WebSocket ERROR for client #%u\n", client->id());
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
    
    // Configure WebSocket with proper settings
    Serial.println("Configuring WebSocket...");
    
    // Setup WebSocket event handler
    ws.onEvent(onWebSocketEvent);
    
    // Add WebSocket handler to server
    webServer.addHandler(&ws);
    
    Serial.printf("WebSocket server configured at /ws (clients: %d)\n", ws.count());
    
    // Handle /dashboard explicitly (redirect to root or serve dashboard.html)
    webServer.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/dashboard.html", "text/html");
    });
    
    // API endpoints - MUST be registered BEFORE serveStatic!
    webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"uptime\":" + String(millis()) + "}");
    });
    
    webServer.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateSensorsJSON());
    });
    
    // Delete/forget a client
    webServer.on("^\\/api\\/clients\\/([0-9]+)$", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        String clientIdStr = request->pathArg(0);
        uint8_t clientId = clientIdStr.toInt();
        
        bool success = forgetClient(clientId);
        
        String response = success ? 
            "{\"success\":true,\"message\":\"Client forgotten\"}" : 
            "{\"success\":false,\"error\":\"Client not found\"}";
        
        request->send(success ? 200 : 404, "application/json", response);
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
    
    // Configuration pages - explicit routes needed since serveStatic can't handle /alerts -> /alerts.html mapping
    // Use beginResponse/endResponse for proper connection management
    // webServer.on("/alerts", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/alerts.html", "text/html");
    //     response->addHeader("Connection", "close");
    //     request->send(response);
    // });

    webServer.on("/alerts", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/alerts.html", "text/html");
    });    
    
    webServer.on("/security", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/security.html", "text/html");
    });
    
    webServer.on("/lora-settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/lora-settings.html", "text/html");
    });
    
    webServer.on("/runtime-config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/runtime-config.html", "text/html");
    });
    
    // LoRa configuration API endpoints
    webServer.on("/api/lora/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        BaseStationConfig config = configStorage.getBaseStationConfig();
        
        // Read current LoRa parameters from NVS
        Preferences prefs;
        prefs.begin("lora_params", true);  // Read-only
        uint32_t frequency = prefs.getUInt("frequency", 915000000);  // Default 915 MHz
        uint8_t spreadingFactor = prefs.getUChar("sf", 10);  // Default SF10
        uint32_t bandwidth = prefs.getUInt("bandwidth", 125000);  // Default 125 kHz
        uint8_t txPower = prefs.getUChar("tx_power", 14);  // Default 14 dBm
        uint8_t codingRate = prefs.getUChar("coding_rate", 1);  // Default 4/5
        uint8_t preambleLength = prefs.getUChar("preamble", 8);  // Default 8
        prefs.end();
        
        String json = "{";
        json += "\"networkId\":" + String(config.networkId) + ",";
        json += "\"region\":\"US915\",";  // Placeholder - would need to add to config
        json += "\"frequency\":" + String(frequency) + ",";
        json += "\"spreadingFactor\":" + String(spreadingFactor) + ",";
        json += "\"bandwidth\":" + String(bandwidth / 1000) + ",";  // Convert Hz to kHz
        json += "\"txPower\":" + String(txPower) + ",";
        json += "\"codingRate\":" + String(codingRate) + ",";
        json += "\"preambleLength\":" + String(preambleLength);
        json += "}";
        request->send(200, "application/json", json);
    });
    
#ifdef BASE_STATION
    webServer.on("/api/lora/config", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            Serial.printf("\n=== LoRa Configuration Update Request ===\n");
            Serial.printf("Payload: %s\n", body.c_str());
            
            // Parse JSON using ArduinoJson
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, body);
            
            if (error) {
                Serial.printf("JSON parse error: %s\n", error.c_str());
                String response = "{\"success\":false,\"error\":\"Invalid JSON\"}";
                request->send(400, "application/json", response);
                return;
            }
            
            // Extract parameters
            uint32_t frequency = doc["frequency"] | 868000000;
            uint8_t spreadingFactor = doc["spreadingFactor"] | 10;
            uint32_t bandwidth = doc["bandwidth"] | 125;
            uint8_t txPower = doc["txPower"] | 14;
            uint8_t codingRate = doc["codingRate"] | 1;
            
            // Convert bandwidth from kHz to Hz
            // Web UI sends 125, 250, or 500 (in kHz)
            if (bandwidth < 1000) {
                bandwidth = bandwidth * 1000;  // Convert kHz to Hz
            }
            
            Serial.println("Parsed Parameters:");
            Serial.printf("  Frequency: %u Hz\n", frequency);
            Serial.printf("  Spreading Factor: SF%d\n", spreadingFactor);
            Serial.printf("  Bandwidth: %u Hz\n", bandwidth);
            Serial.printf("  TX Power: %d dBm\n", txPower);
            Serial.printf("  Coding Rate: 4/%d\n", codingRate + 4);
            
            // Save to base station's NVS for application after reboot
            Preferences prefs;
            prefs.begin("lora_params", false);
            prefs.putUInt("frequency", frequency);
            prefs.putUChar("sf", spreadingFactor);
            prefs.putUInt("bandwidth", bandwidth);
            prefs.putUChar("tx_power", txPower);
            prefs.putUChar("coding_rate", codingRate);
            prefs.putBool("pending", true);
            prefs.end();
            
            Serial.println("✓ Parameters saved to base station NVS");
            
            // Send SET_LORA_PARAMS command to all active sensor nodes
            extern RemoteConfigManager remoteConfigManager;
            int sensorCount = 0;
            int commandsSent = 0;
            
            Serial.println("\n=== Broadcasting to Sensor Nodes ===");
            for (int i = 0; i < 256; i++) {
                SensorInfo* sensor = getSensorInfo(i);
                if (sensor != NULL && !isSensorTimedOut(i)) {
                    sensorCount++;
                    Serial.printf("Sending SET_LORA_PARAMS to sensor %d (%s)...\n", 
                                 i, sensor->location);
                    
                    // Use CommandBuilder to create the command
                    CommandPacket cmd = CommandBuilder::createSetLoRaParams(
                        i, frequency, spreadingFactor, bandwidth, txPower, codingRate
                    );
                    
                    // Queue the command (will be sent via existing remote config system)
                    uint8_t cmdData[14];
                    memcpy(&cmdData[0], &frequency, sizeof(uint32_t));
                    cmdData[4] = spreadingFactor;
                    memcpy(&cmdData[5], &bandwidth, sizeof(uint32_t));
                    cmdData[9] = txPower;
                    cmdData[10] = codingRate;
                    
                    if (remoteConfigManager.queueCommand(i, CMD_SET_LORA_PARAMS, cmdData, 11)) {
                        commandsSent++;
                        Serial.printf("  ✓ Command queued for sensor %d\n", i);
                    } else {
                        Serial.printf("  ✗ Failed to queue command for sensor %d\n", i);
                    }
                }
            }
            
            Serial.println("===================================");
            Serial.printf("Commands sent to %d of %d active sensors\n", commandsSent, sensorCount);
            
            // Initialize reboot coordination tracker
            loraRebootTracker.sensorAcks.clear();
            loraRebootTracker.totalSensors = commandsSent;
            loraRebootTracker.commandStartTime = millis();
            loraRebootTracker.trackingActive = (commandsSent > 0);
            
            // Record which sensors we're waiting for
            for (int i = 0; i < 256; i++) {
                SensorInfo* sensor = getSensorInfo(i);
                if (sensor != NULL && !isSensorTimedOut(i)) {
                    loraRebootTracker.sensorAcks[i] = false;
                    Serial.printf("Tracking ACK from sensor %d (%s)\n", i, sensor->location);
                }
            }
            
            Serial.println("\n⚠️  COORDINATION PROTOCOL:");
            Serial.println("1. Waiting for all sensors to ACK (max 20s)");
            Serial.println("2. Sensors will auto-reboot 5s after ACK");
            Serial.println("3. Base station will reboot after all ACKs + 5s");
            Serial.println("4. All nodes will apply new LoRa parameters");
            Serial.println("===================================\n");
            
            // Build JSON response with details
            StaticJsonDocument<512> responseDoc;
            responseDoc["success"] = true;
            responseDoc["message"] = "LoRa parameters updated and broadcast to sensors";
            responseDoc["sensorsFound"] = sensorCount;
            responseDoc["commandsSent"] = commandsSent;
            responseDoc["rebootRequired"] = true;
            responseDoc["trackingEnabled"] = loraRebootTracker.trackingActive;
            
            String response;
            serializeJson(responseDoc, response);
            request->send(200, "application/json", response);
            
            // DON'T auto-schedule reboot yet - wait for ACKs
            Serial.println("⏳ Waiting for sensor ACKs before scheduling reboot...");
            Serial.println("Use /api/lora/reboot-status to monitor progress.");
        });
    
    // API endpoint to check LoRa reboot coordination status
    webServer.on("/api/lora/reboot-status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;
        
        doc["trackingActive"] = loraRebootTracker.trackingActive;
        doc["totalSensors"] = loraRebootTracker.totalSensors;
        doc["commandStartTime"] = loraRebootTracker.commandStartTime;
        doc["elapsedTime"] = loraRebootTracker.trackingActive ? (millis() - loraRebootTracker.commandStartTime) : 0;
        
        // Count ACKs
        uint8_t ackedCount = 0;
        JsonArray sensors = doc.createNestedArray("sensors");
        for (auto& pair : loraRebootTracker.sensorAcks) {
            JsonObject sensor = sensors.createNestedObject();
            sensor["id"] = pair.first;
            sensor["acked"] = pair.second;
            if (pair.second) ackedCount++;
            
            SensorInfo* info = getSensorInfo(pair.first);
            if (info != NULL) {
                sensor["name"] = info->location;
            }
        }
        
        doc["ackedCount"] = ackedCount;
        doc["allAcked"] = (ackedCount == loraRebootTracker.totalSensors) && (loraRebootTracker.totalSensors > 0);
        
        // Check if we've timed out (20 seconds)
        bool timedOut = loraRebootTracker.trackingActive && (millis() - loraRebootTracker.commandStartTime > 20000);
        doc["timedOut"] = timedOut;
        
        // Check if base station reboot is scheduled
        extern bool loraRebootPending;
        extern uint32_t loraRebootTime;
        doc["rebootPending"] = loraRebootPending;
        if (loraRebootPending) {
            doc["rebootIn"] = (loraRebootTime > millis()) ? (loraRebootTime - millis()) / 1000 : 0;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    #endif // BASE_STATION
    
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
    
    #ifdef BASE_STATION
    // MQTT configuration page - explicit route needed
    webServer.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/mqtt.html", "text/html");
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
    
    // Time & NTP configuration page
    webServer.on("/time", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/time.html", "text/html");
    });
    
    // Time API endpoints
    webServer.on("/api/time/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        NTPConfig cfg = configStorage.getNTPConfig();
        
        String json = "{";
        json += "\"enabled\":" + String(cfg.enabled ? "true" : "false") + ",";
        json += "\"server\":\"" + String(cfg.server) + "\",";
        json += "\"intervalSec\":" + String(cfg.intervalSec) + ",";
        json += "\"tzOffsetMinutes\":" + String(cfg.tzOffsetMinutes);
        json += "}";
        request->send(200, "application/json", json);
    });
    
    webServer.on("/api/time/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, body);
            
            if (error) {
                request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                return;
            }
            
            NTPConfig cfg;
            cfg.enabled = doc["enabled"] | false;
            String server = doc["server"] | "pool.ntp.org";
            server.toCharArray(cfg.server, sizeof(cfg.server));
            cfg.intervalSec = doc["intervalSec"] | 3600;
            cfg.tzOffsetMinutes = doc["tzOffsetMinutes"] | 0;
            
            configStorage.setNTPConfig(cfg);
            
            // Reconfigure NTP if enabled
            if (cfg.enabled) {
                long gmtOffsetSec = (long)cfg.tzOffsetMinutes * 60;
                configTime(gmtOffsetSec, 0, cfg.server);
                Serial.printf("NTP reconfigured: %s, offset=%d min\n", cfg.server, cfg.tzOffsetMinutes);
            }
            
            request->send(200, "application/json", "{\"success\":true}");
        });
    
    webServer.on("/api/time/sync", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            time_t now = time(nullptr);
            if (now < 1000000000) {
                request->send(500, "application/json", "{\"success\":false,\"error\":\"NTP not synced\"}");
                return;
            }
            
            NTPConfig cfg = configStorage.getNTPConfig();
            uint8_t payload[6];
            memcpy(&payload[0], &now, sizeof(uint32_t));
            int16_t tz = cfg.tzOffsetMinutes;
            memcpy(&payload[4], &tz, sizeof(int16_t));
            
            extern RemoteConfigManager remoteConfigManager;
            extern uint8_t getActiveClientCount();
            extern ClientInfo* getClientByIndex(uint8_t index);
            
            int sent = 0;
            uint8_t activeCount = getActiveClientCount();
            for (uint8_t i = 0; i < activeCount; i++) {
                ClientInfo* client = getClientByIndex(i);
                if (client && client->active) {
                    if (remoteConfigManager.queueCommand(client->clientId, CMD_TIME_SYNC, payload, 6)) {
                        sent++;
                    }
                }
            }
            
            String response = "{\"success\":true,\"commandsSent\":" + String(sent) + 
                            ",\"epoch\":" + String((unsigned long)now) + 
                            ",\"tzOffset\":" + String((int)tz) + "}";
            request->send(200, "application/json", response);
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
    
    webServer.on("/api/remote-config/location", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleRemoteSetLocation(request, data, len);
        });
    
    webServer.on("/api/remote-config/get-config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleRemoteGetConfig(request, data, len);
        });
    
    webServer.on("/api/remote-config/queue-status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", generateCommandQueueJSON());
    });
    
    // ============================================================================
    // SENSOR ZONE AND PRIORITY API ENDPOINTS
    // ============================================================================
    
    // Get sensor zone
    webServer.on("^\\/api\\/sensors\\/([0-9]+)\\/zone$", HTTP_GET, [](AsyncWebServerRequest *request) {
        extern SensorConfigManager sensorConfigManager;
        uint8_t sensorId = request->pathArg(0).toInt();
        String zone = sensorConfigManager.getSensorZone(sensorId);
        String json = "{\"sensorId\":" + String(sensorId) + ",\"zone\":\"" + zone + "\"}";
        request->send(200, "application/json", json);
    });
    
    // Set sensor zone
    webServer.on("^\\/api\\/sensors\\/([0-9]+)\\/zone$", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            extern SensorConfigManager sensorConfigManager;
            uint8_t sensorId = request->pathArg(0).toInt();
            
            String body = String((char*)data).substring(0, len);
            int zoneStart = body.indexOf("\"zone\":\"") + 8;
            int zoneEnd = body.indexOf("\"", zoneStart);
            String zone = body.substring(zoneStart, zoneEnd);
            
            bool success = sensorConfigManager.setSensorZone(sensorId, zone.c_str());
            String response = success ? 
                "{\"success\":true,\"message\":\"Zone updated\"}" : 
                "{\"success\":false,\"error\":\"Failed to update zone\"}";
            request->send(success ? 200 : 500, "application/json", response);
        });
    
    // Get sensor priority
    webServer.on("^\\/api\\/sensors\\/([0-9]+)\\/priority$", HTTP_GET, [](AsyncWebServerRequest *request) {
        extern SensorConfigManager sensorConfigManager;
        uint8_t sensorId = request->pathArg(0).toInt();
        SensorPriority priority = sensorConfigManager.getSensorPriority(sensorId);
        const char* priorityStr = (priority == PRIORITY_HIGH) ? "High" : (priority == PRIORITY_LOW) ? "Low" : "Medium";
        String json = "{\"sensorId\":" + String(sensorId) + ",\"priority\":\"" + String(priorityStr) + "\",\"level\":" + String((int)priority) + "}";
        request->send(200, "application/json", json);
    });
    
    // Set sensor priority
    webServer.on("^\\/api\\/sensors\\/([0-9]+)\\/priority$", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            extern SensorConfigManager sensorConfigManager;
            uint8_t sensorId = request->pathArg(0).toInt();
            
            String body = String((char*)data).substring(0, len);
            int levelStart = body.indexOf("\"level\":") + 8;
            int levelEnd = body.indexOf(",", levelStart);
            if (levelEnd == -1) levelEnd = body.indexOf("}", levelStart);
            int level = body.substring(levelStart, levelEnd).toInt();
            
            SensorPriority priority = (SensorPriority)level;
            bool success = sensorConfigManager.setSensorPriority(sensorId, priority);
            String response = success ? 
                "{\"success\":true,\"message\":\"Priority updated\"}" : 
                "{\"success\":false,\"error\":\"Failed to update priority\"}";
            request->send(success ? 200 : 500, "application/json", response);
        });
    
    // Get sensor health
    webServer.on("^\\/api\\/sensors\\/([0-9]+)\\/health$", HTTP_GET, [](AsyncWebServerRequest *request) {
        extern SensorConfigManager sensorConfigManager;
        uint8_t sensorId = request->pathArg(0).toInt();
        SensorHealthScore health = sensorConfigManager.getHealthScore(sensorId);
        
        String json = "{";
        json += "\"sensorId\":" + String(sensorId) + ",";
        json += "\"overallHealth\":" + String(health.overallHealth, 2) + ",";
        json += "\"communicationReliability\":" + String(health.communicationReliability, 2) + ",";
        json += "\"batteryHealth\":" + String(health.batteryHealth, 2) + ",";
        json += "\"readingQuality\":" + String(health.readingQuality, 2) + ",";
        json += "\"uptimeSeconds\":" + String(health.uptimeSeconds) + ",";
        json += "\"lastSeenTimestamp\":" + String(health.lastSeenTimestamp) + ",";
        json += "\"totalPackets\":" + String(health.totalPackets) + ",";
        json += "\"failedPackets\":" + String(health.failedPackets);
        json += "}";
        request->send(200, "application/json", json);
    });
    
    // ============================================================================
    // SECURITY API ENDPOINTS
    // ============================================================================
    
    // Get security configuration
    webServer.on("/api/security/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        SecurityConfig config = securityManager.getConfig();
        String json = "{";
        json += "\"encryptionEnabled\":" + String(config.encryptionEnabled ? "true" : "false") + ",";
        json += "\"whitelistEnabled\":" + String(config.whitelistEnabled ? "true" : "false") + ",";
        json += "\"sequenceNumber\":" + String(config.sequenceNumber);
        json += "}";
        request->send(200, "application/json", json);
    });
    
    // Update security configuration
    webServer.on("/api/security/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            Serial.printf("Security config update: %s\n", body.c_str());
            
            // Parse JSON
            int encIdx = body.indexOf("\"encryptionEnabled\":");
            int wlIdx = body.indexOf("\"whitelistEnabled\":");
            
            if (encIdx != -1) {
                bool encEnabled = body.indexOf("true", encIdx) != -1;
                securityManager.setEncryptionEnabled(encEnabled);
                Serial.printf("Encryption %s\n", encEnabled ? "ENABLED" : "DISABLED");
            }
            
            if (wlIdx != -1) {
                bool wlEnabled = body.indexOf("true", wlIdx) != -1;
                securityManager.setWhitelistEnabled(wlEnabled);
                Serial.printf("Whitelist %s\n", wlEnabled ? "ENABLED" : "DISABLED");
            }
            
            if (securityManager.saveConfig()) {
                request->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save config\"}");
            }
        });
    
    // Get whitelist
    webServer.on("/api/security/whitelist", HTTP_GET, [](AsyncWebServerRequest *request) {
        uint8_t whitelist[MAX_WHITELIST_SIZE];
        uint8_t count = securityManager.getWhitelist(whitelist, MAX_WHITELIST_SIZE);
        
        String json = "{\"count\":" + String(count) + ",\"devices\":[";
        for (uint8_t i = 0; i < count; i++) {
            if (i > 0) json += ",";
            json += String(whitelist[i]);
        }
        json += "]}";
        request->send(200, "application/json", json);
    });
    
    // Add device to whitelist
    webServer.on("/api/security/whitelist", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            Serial.printf("Whitelist add: %s\n", body.c_str());
            
            // Parse device ID from JSON: {"deviceId": 123}
            int idIdx = body.indexOf("\"deviceId\":");
            if (idIdx != -1) {
                int numStart = idIdx + 11;
                int commaIdx = body.indexOf(',', numStart);
                int braceIdx = body.indexOf('}', numStart);
                int numEnd = (commaIdx != -1 && commaIdx < braceIdx) ? commaIdx : braceIdx;
                String idStr = body.substring(numStart, numEnd);
                uint8_t deviceId = idStr.toInt();
                
                if (securityManager.addToWhitelist(deviceId)) {
                    if (securityManager.saveConfig()) {
                        Serial.printf("? Device %d added to whitelist\n", deviceId);
                        request->send(200, "application/json", "{\"status\":\"success\"}");
                    } else {
                        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save\"}");
                    }
                } else {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Whitelist full or duplicate\"}");
                }
            } else {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request\"}");
            }
        });
    
    // Remove device from whitelist
    webServer.on("^\\/api\\/security\\/whitelist\\/([0-9]+)$", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        String deviceIdStr = request->pathArg(0);
        uint8_t deviceId = deviceIdStr.toInt();
        
        if (securityManager.removeFromWhitelist(deviceId)) {
            if (securityManager.saveConfig()) {
                Serial.printf("? Device %d removed from whitelist\n", deviceId);
                request->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save\"}");
            }
        } else {
            request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"Device not found\"}");
        }
    });
    
    // Clear entire whitelist
    webServer.on("/api/security/whitelist", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        securityManager.clearWhitelist();
        if (securityManager.saveConfig()) {
            Serial.println("? Whitelist cleared");
            request->send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save\"}");
        }
    });
    
    // Get encryption key
    webServer.on("/api/security/key", HTTP_GET, [](AsyncWebServerRequest *request) {
        uint8_t key[16];
        securityManager.getKey(key);
        
        // Convert to hex string
        String hexKey = "";
        for (int i = 0; i < 16; i++) {
            char hex[3];
            sprintf(hex, "%02X", key[i]);
            hexKey += hex;
        }
        
        String json = "{\"key\":\"" + hexKey + "\"}";
        request->send(200, "application/json", json);
    });
    
    // Set encryption key from hex string
    webServer.on("/api/security/key", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body = String((char*)data);
        
        // Parse hex key from JSON
        int keyStart = body.indexOf("\"key\"") + 7;
        int keyEnd = body.indexOf("\"", keyStart);
        String hexKey = body.substring(keyStart, keyEnd);
        
        // Validate hex key (must be 32 hex characters = 16 bytes)
        if (hexKey.length() != 32) {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Key must be 32 hex characters\"}");
            return;
        }
        
        // Convert hex string to bytes
        uint8_t key[16];
        for (int i = 0; i < 16; i++) {
            String byteStr = hexKey.substring(i*2, i*2+2);
            key[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
        }
        
        securityManager.setKey(key);
        if (securityManager.saveConfig()) {
            Serial.println("?? Encryption key updated");
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Key updated\"}");
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save\"}");
        }
    });
    
    // Generate new encryption key
    webServer.on("/api/security/generate-key", HTTP_POST, [](AsyncWebServerRequest *request) {
        securityManager.generateKey();
        if (securityManager.saveConfig()) {
            // Get the new key to return it
            uint8_t key[16];
            securityManager.getKey(key);
            
            // Convert to hex string
            String hexKey = "";
            for (int i = 0; i < 16; i++) {
                char hex[3];
                sprintf(hex, "%02X", key[i]);
                hexKey += hex;
            }
            
            Serial.println("?? New encryption key generated");
            String json = "{\"status\":\"success\",\"message\":\"New key generated\",\"key\":\"" + hexKey + "\"}";
            request->send(200, "application/json", json);
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save\"}");
        }
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

/**
 * @brief Cleanup disconnected WebSocket clients
 * 
 * Must be called periodically (every 1-2 seconds) to free resources
 */
void WiFiPortal::cleanupWebSocket() {
    if (dashboardActive) {
        ws.cleanupClients();
    }
}

/**
 * @brief Sanitize a string to ensure it's valid UTF-8 and JSON-safe
 * Replaces invalid characters with underscores and ensures proper null termination
 */
String sanitizeString(const char* str) {
    if (str == nullptr) {
        return String("");
    }
    
    // Check if string appears to be garbage/uninitialized
    // If first byte is 0, it's empty
    if (str[0] == '\0') {
        return String("");
    }
    
    String result = "";
    result.reserve(64);  // Pre-allocate
    
    for (size_t i = 0; i < 32 && str[i] != '\0'; i++) {  // Max 32 chars (size of location field)
        unsigned char c = (unsigned char)str[i];
        
        // Only allow printable ASCII (32-126)
        if (c >= 32 && c <= 126) {
            // Escape JSON special characters
            if (c == '"') {
                result += "\\\"";
            } else if (c == '\\') {
                result += "\\\\";
            } else if (c == '\n') {
                result += "\\n";
            } else if (c == '\r') {
                result += "\\r";
            } else if (c == '\t') {
                result += "\\t";
            } else {
                result += (char)c;
            }
        } else {
            // Skip control characters and high-bit bytes (potential corruption)
            // Don't add anything for these
        }
    }
    
    // If result is empty or too short, it was probably garbage
    if (result.length() == 0) {
        return String("");
    }
    
    return result;
}

String WiFiPortal::generateSensorsJSON() {
#ifdef BASE_STATION
    extern SensorConfigManager sensorConfigManager;
#endif
    
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
            
#ifdef BASE_STATION
            // Get metadata for zone, priority, and health
            SensorMetadata meta = sensorConfigManager.getSensorMetadata(sensor->sensorId);
            SensorHealthScore health = sensorConfigManager.getHealthScore(sensor->sensorId);
            
            // Priority string
            const char* priorityStr = "Medium";
            if (meta.priority == PRIORITY_HIGH) priorityStr = "High";
            else if (meta.priority == PRIORITY_LOW) priorityStr = "Low";
#endif
            
            json += "{";
            json += "\"id\":" + String(sensor->sensorId) + ",";
            json += "\"location\":\"" + sanitizeString(sensor->location) + "\",";
#ifdef BASE_STATION
            json += "\"zone\":\"" + sanitizeString(sensor->zone) + "\",";
            json += "\"priority\":\"" + String(priorityStr) + "\",";
            json += "\"priorityLevel\":" + String((int)meta.priority) + ",";
#endif
            json += "\"battery\":" + String(sensor->lastBatteryPercent) + ",";
            json += "\"charging\":" + String(sensor->powerState ? "true" : "false") + ",";
            json += "\"rssi\":" + String(sensor->lastRssi) + ",";
            json += "\"snr\":" + String(sensor->lastSnr) + ",";
            json += "\"packets\":" + String(sensor->packetsReceived) + ",";
            json += "\"ageSeconds\":" + String(ageSeconds) + ",";
            json += "\"age\":\"" + ageStr + "\"";
#ifdef BASE_STATION
            json += ",\"health\":{";
            json += "\"overall\":" + String(health.overallHealth, 2) + ",";
            json += "\"communication\":" + String(health.communicationReliability, 2) + ",";
            json += "\"battery\":" + String(health.batteryHealth, 2) + ",";
            json += "\"quality\":" + String(health.readingQuality, 2) + ",";
            json += "\"totalPackets\":" + String(health.totalPackets) + ",";
            json += "\"failedPackets\":" + String(health.failedPackets);
            json += "}";
#endif
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
    uint32_t cutoffTime = 0;
    
    // Only apply time filter if we have enough uptime to cover the range
    if (timeRange > 0) {
        if (currentTime > timeRange) {
            cutoffTime = currentTime - timeRange;
        }
        // else cutoffTime stays 0, showing all data when uptime < requested range
    }
    
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

#ifdef BASE_STATION
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

void WiFiPortal::handleRemoteSetLocation(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    extern RemoteConfigManager remoteConfigManager;
    
    Serial.println("=== handleRemoteSetLocation called ===");
    
    // Build body string
    String body = "";
    body.reserve(len + 1);
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    Serial.printf("Received JSON: %s\n", body.c_str());
    
    // Parse JSON: {"id":1,"location":"Kitchen"}
    int idStart = body.indexOf("\"id\":") + 5;
    int idEnd = body.indexOf(",", idStart);
    uint8_t sensorId = body.substring(idStart, idEnd).toInt();
    
    int locationStart = body.indexOf("\"location\":\"") + 12;
    int locationEnd = body.lastIndexOf("\"");
    String location = body.substring(locationStart, locationEnd);
    
    Serial.printf("Remote config: Set location for sensor %d to '%s'\n", sensorId, location.c_str());
    
    // Validate location length (max 31 characters + null terminator)
    if (location.length() > 31) {
        Serial.println("ERROR: Location too long (max 31 characters)");
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Location too long (max 31 characters)\"}");
        return;
    }
    
    // Queue command with location string as data
    uint8_t locationData[32];
    strncpy((char*)locationData, location.c_str(), 31);
    locationData[31] = '\0';  // Ensure null termination
    
    bool success = remoteConfigManager.queueCommand(sensorId, CMD_SET_LOCATION, locationData, strlen((char*)locationData) + 1);
    
    // Also update the base station's stored sensor metadata immediately
    #ifdef BASE_STATION
    extern SensorConfigManager sensorConfigManager;
    SensorMetadata metadata = sensorConfigManager.getSensorMetadata(sensorId);
    strncpy(metadata.location, location.c_str(), sizeof(metadata.location) - 1);
    metadata.location[sizeof(metadata.location) - 1] = '\0';
    sensorConfigManager.setSensorMetadata(sensorId, metadata);
    Serial.printf("Updated base station metadata for sensor %d\n", sensorId);
    #endif
    
    String response = success ? 
        "{\"success\":true,\"message\":\"Location command queued\"}" : 
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

// Global function to update LoRa reboot tracking when sensor ACKs
// This is called from lora_comm.cpp when ACK is received
void updateLoRaRebootTracking(uint8_t sensorId) {
    if (!loraRebootTracker.trackingActive) return;
    
    if (loraRebootTracker.sensorAcks.find(sensorId) != loraRebootTracker.sensorAcks.end()) {
        loraRebootTracker.sensorAcks[sensorId] = true;
        Serial.printf("✅ Sensor %d ACKed LoRa settings command\n", sensorId);
        
        // Count how many have ACKed
        uint8_t ackedCount = 0;
        for (auto& pair : loraRebootTracker.sensorAcks) {
            if (pair.second) ackedCount++;
        }
        
        Serial.printf("Progress: %d/%d sensors ACKed\n", ackedCount, loraRebootTracker.totalSensors);
        
        // Check if all sensors have ACKed
        if (ackedCount == loraRebootTracker.totalSensors) {
            Serial.println("\n========================================");
            Serial.println("✅ ALL SENSORS ACKNOWLEDGED!");
            Serial.println("Sensors will reboot in ~5 seconds");
            Serial.println("Scheduling base station reboot in 8 seconds...");
            Serial.println("========================================\n");
            
            extern bool loraRebootPending;
            extern uint32_t loraRebootTime;
            loraRebootPending = true;
            loraRebootTime = millis() + 8000;  // 8 seconds (sensors reboot at 5s)
            loraRebootTracker.trackingActive = false;
        }
    }
}

// Check if we've timed out waiting for sensor ACKs (20 seconds)
void checkLoRaRebootTimeout() {
    if (!loraRebootTracker.trackingActive) return;
    
    // Check if 20 seconds have elapsed
    if (millis() - loraRebootTracker.commandStartTime > 20000) {
        // Count how many ACKed
        uint8_t ackedCount = 0;
        for (auto& pair : loraRebootTracker.sensorAcks) {
            if (pair.second) ackedCount++;
        }
        
        Serial.println("\n========================================");
        Serial.println("⚠️  TIMEOUT WAITING FOR SENSOR ACKS");
        Serial.printf("Received ACKs from %d/%d sensors\n", ackedCount, loraRebootTracker.totalSensors);
        Serial.println("Proceeding with base station reboot anyway...");
        Serial.println("========================================\n");
        
        // Schedule reboot even though not all sensors ACKed
        extern bool loraRebootPending;
        extern uint32_t loraRebootTime;
        loraRebootPending = true;
        loraRebootTime = millis() + 5000;  // 5 second grace period
        loraRebootTracker.trackingActive = false;
    }
}
#endif // BASE_STATION

// ============================================================================
// DIAGNOSTICS FUNCTIONS (for link test feature)
// ============================================================================

void WiFiPortal::diagnosticsRecordSent(uint8_t sensorId, uint8_t sequenceNumber) {
    // Track that we sent a ping/command to this sensor
    // This is called from lora_comm.cpp when sending diagnostic packets
    Serial.printf("📤 Diagnostic sent to sensor %d, seq %d\n", sensorId, sequenceNumber);
}

void WiFiPortal::diagnosticsRecordAck(uint8_t sensorId, uint8_t sequenceNumber, int16_t rssi, int8_t snr) {
    // Track that we received an ACK from this sensor
    // This is called from lora_comm.cpp when receiving diagnostic ACK packets
    Serial.printf("📥 Diagnostic ACK from sensor %d, seq %d, RSSI=%d, SNR=%d\n", 
                 sensorId, sequenceNumber, rssi, snr);
}
