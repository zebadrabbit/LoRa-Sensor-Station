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
    
    // Check if portal is running
    bool isPortalActive();
    
    // Process DNS and web server requests
    void handleClient();
    
    // Get current AP IP address
    String getAPIP();
    
private:
    DNSServer dnsServer;
    AsyncWebServer webServer;
    bool portalActive;
    
    // Setup web server routes
    void setupWebServer();
    
    // HTML page handlers
    String generateMainPage();
    String generateSensorConfigPage();
    String generateBaseStationConfigPage();
    String generateSuccessPage(const String& message);
    
    // Handle form submissions
    void handleModeSelection(AsyncWebServerRequest *request);
    void handleSensorConfig(AsyncWebServerRequest *request);
    void handleBaseStationConfig(AsyncWebServerRequest *request);
};

// Global instance
extern WiFiPortal wifiPortal;

#endif // WIFI_PORTAL_H
