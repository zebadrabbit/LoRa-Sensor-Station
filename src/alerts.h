/**
 * @file alerts.h
 * @brief Alert management system for LoRa sensor network
 * @version 2.5.0
 * @date 2025-12-12
 * 
 * Manages alert notifications via multiple channels:
 * - Microsoft Teams webhooks
 * - Email notifications (SMTP)
 * 
 * Features:
 * - Configurable thresholds (temperature, battery)
 * - Rate limiting to prevent alert spam
 * - Dual-channel delivery (succeeds if either works)
 * - Persistent configuration storage (NVS)
 * - Test functionality for each notification channel
 */

#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/**
 * @enum AlertType
 * @brief Types of alerts that can be triggered
 */
enum AlertType {
    ALERT_TEMPERATURE_HIGH,
    ALERT_TEMPERATURE_LOW,
    ALERT_BATTERY_LOW,
    ALERT_BATTERY_CRITICAL,
    ALERT_SENSOR_OFFLINE,
    ALERT_SENSOR_ONLINE,
    ALERT_COMMUNICATION_FAILURE,
    ALERT_SYSTEM_ERROR
};

// Alert configuration structure
struct AlertConfig {
    bool teamsEnabled;
    char teamsWebhook[256];
    
    // Email configuration
    bool emailEnabled;
    char smtpServer[64];
    uint16_t smtpPort;
    char emailUser[64];
    char emailPassword[64];
    char emailFrom[64];
    char emailTo[128];
    bool emailTLS;
    
    // Temperature thresholds
    float tempHighThreshold;
    float tempLowThreshold;
    
    // Battery thresholds
    uint8_t batteryLowThreshold;
    uint8_t batteryCriticalThreshold;
    
    // Sensor timeout (minutes)
    uint16_t sensorTimeoutMinutes;
    
    // Rate limiting (seconds between same alert type)
    uint32_t rateLimitSeconds;
    
    // Alert enable flags
    bool alertTempHigh;
    bool alertTempLow;
    bool alertBatteryLow;
    bool alertBatteryCritical;
    bool alertSensorOffline;
    bool alertSensorOnline;
};

// Alert tracking structure
struct AlertState {
    uint32_t lastAlertTime[20];  // Last alert time per sensor (max 20 sensors)
    AlertType lastAlertType[20];  // Last alert type per sensor
};

class AlertManager {
public:
    AlertManager();
    
    // Configuration
    void begin();
    void loadConfig();
    void saveConfig();
    AlertConfig* getConfig();
    void setTeamsWebhook(const char* webhook);
    void setTemperatureThresholds(float low, float high);
    void setBatteryThresholds(uint8_t low, uint8_t critical);
    void setSensorTimeout(uint16_t minutes);
    void setRateLimit(uint32_t seconds);
    void enableAlert(AlertType type, bool enabled);
    
    // Alert sending
    bool sendAlert(uint8_t sensorId, AlertType type, const String& message, const String& details = "");
    bool sendTeamsAlert(const String& title, const String& message, const String& color = "0078D4");
    
#ifdef BASE_STATION
    bool sendEmailAlert(const String& subject, const String& message);
    bool testEmailSettings();
#else
    // Stub implementations for sensor nodes (no email support)
    inline bool sendEmailAlert(const String& subject, const String& message) { return false; }
    inline bool testEmailSettings() { return false; }
#endif
    
    // Alert checking
    void checkSensorAlerts(uint8_t sensorId, float temperature, uint8_t battery, bool online);
    void checkAllSensors();
    
    // Rate limiting
    bool shouldSendAlert(uint8_t sensorId, AlertType type);
    
    // Test functions
    bool testTeamsWebhook();
    
private:
    AlertConfig config;
    AlertState state;
    bool initialized;
    
    String getAlertTitle(AlertType type);
    String getAlertColor(AlertType type);
    String formatTeamsCard(const String& title, const String& message, const String& color, const String& details);
};

extern AlertManager alertManager;

#endif // ALERTS_H
