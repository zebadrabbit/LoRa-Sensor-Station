#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "config_storage.h"

class WiFiPortal {
public:
    WiFiPortal();
    
    // Start captive portal in AP mode
    void startPortal();
    
    // Stop portal and connect to WiFi (for base station)
    bool connectToWiFi(const char* ssid, const char* password);
    
    // Start web dashboard (base station mode)
    void startDashboard();
    
    // Check if portal is running
    bool isPortalActive();
    
    // Check if dashboard is running
    bool isDashboardActive();
    
    // Process DNS and web server requests
    void handleClient();
    
    // Get current AP IP address
    String getAPIP();
    
    // WebSocket broadcast for real-time updates
    void broadcastSensorUpdate();
    
    // WebSocket cleanup (call periodically)
    void cleanupWebSocket();
    
    // Public JSON generators (used by WebSocket)
    String generateSensorsJSON();
    String generateStatsJSON();
    
    // Diagnostics hooks (used by LoRa comms)
    void diagnosticsRecordSent(uint8_t sensorId, uint8_t sequenceNumber);
    void diagnosticsRecordAck(uint8_t sensorId, uint8_t sequenceNumber, int16_t rssi, int8_t snr);

    // Optional: check if diagnostics are active
    bool isDiagnosticsActive();

private:
    DNSServer dnsServer;
    AsyncWebServer webServer;
    bool portalActive;
    bool dashboardActive;
    
    // WiFi connection helpers
    bool isNetworkUsable();
    void resetWiFiStack();
    
    // Setup web server routes
    void setupWebServer();
    
    // Setup dashboard routes (base station)
    void setupDashboard();
    
    // HTML page handlers
    String generateMainPage();
    String generateSensorConfigPage();
    String generateBaseStationConfigPage();
    String generateSuccessPage(const String& message);
    
    // Dashboard page handlers
    String generateDashboardPage();
    String generateHistoryJSON(uint8_t sensorId, uint32_t timeRange);
    String generateAlertsPage();
    String generateAlertsConfigJSON();
    String generateMQTTPage();
    String generateMQTTConfigJSON();
    
    // Handle form submissions
    void handleModeSelection(AsyncWebServerRequest *request);
    void handleSensorConfig(AsyncWebServerRequest *request);
    void handleBaseStationConfig(AsyncWebServerRequest *request);
    void handleAlertsConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleMQTTConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleRemoteSetInterval(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleRemoteSetLocation(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleRemoteRestart(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleRemoteGetConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    String generateCommandQueueJSON();
    
    // Alert testing
    bool testTeamsWebhook();
};

// Global instance
extern WiFiPortal wifiPortal;

#endif // WIFI_PORTAL_H
