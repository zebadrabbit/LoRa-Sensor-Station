/**
 * @file alerts.cpp
 * @brief Implementation of alert management system
 * @version 2.5.0
 * @date 2025-12-12
 * 
 * Implements dual-channel alert notifications:
 * 
 * 1. Microsoft Teams Integration:
 *    - Sends formatted JSON to Teams webhook
 *    - Includes sensor details, timestamps, and severity
 *    - Color-coded based on alert type
 * 
 * 2. Email Notifications:
 *    - SMTP with TLS/STARTTLS support
 *    - HTML formatted emails with styling
 *    - Plain text fallback for compatibility
 *    - Supports Gmail, Outlook, and other SMTP servers
 * 
 * Features:
 * - Rate limiting (300s default cooldown)
 * - Dual-channel delivery (succeeds if either works)
 * - NVS persistent configuration
 * - Test mode for validation
 * - Per-sensor alert tracking
 */

#include "alerts.h"
#include "config_storage.h"
#include "statistics.h"
#include <Preferences.h>

#ifdef BASE_STATION
#include <ESP_Mail_Client.h>
#include "sensor_config.h"  // For SensorConfigManager
#endif

// Global alert manager instance
AlertManager alertManager;

/**
 * @brief Constructor - Initialize alert manager with default values
 * 
 * Default Configuration:
 * - Teams/Email: Disabled
 * - Temperature: 10°C low, 30°C high
 * - Battery: 20% low, 10% critical
 * - Timeout: 15 minutes
 * - Rate limit: 300 seconds (5 minutes)
 * - All alert types: Enabled (except sensor online)
 */
AlertManager::AlertManager() : initialized(false) {
    // Initialize default configuration
    config.teamsEnabled = false;
    strcpy(config.teamsWebhook, "");
    
    config.emailEnabled = false;
    strcpy(config.smtpServer, "smtp.gmail.com");
    config.smtpPort = 587;
    strcpy(config.emailUser, "");
    strcpy(config.emailPassword, "");
    strcpy(config.emailFrom, "");
    strcpy(config.emailTo, "");
    config.emailTLS = true;
    
    config.tempHighThreshold = 30.0;
    config.tempLowThreshold = 10.0;
    config.batteryLowThreshold = 20;
    config.batteryCriticalThreshold = 10;
    config.sensorTimeoutMinutes = 15;
    config.rateLimitSeconds = 300;  // 5 minutes
    
    config.alertTempHigh = true;
    config.alertTempLow = true;
    config.alertBatteryLow = true;
    config.alertBatteryCritical = true;
    config.alertSensorOffline = true;
    config.alertSensorOnline = false;
    
    // Initialize alert state
    for (int i = 0; i < 20; i++) {
        state.lastAlertTime[i] = 0;
        state.lastAlertType[i] = ALERT_SYSTEM_ERROR;
    }
}

void AlertManager::begin() {
    Serial.println("Initializing Alert Manager...");
    loadConfig();
    initialized = true;
}

void AlertManager::loadConfig() {
    Preferences prefs;
    if (prefs.begin("alerts", true)) {
        config.teamsEnabled = prefs.getBool("teamsEnabled", false);
        prefs.getString("teamsWebhook", config.teamsWebhook, sizeof(config.teamsWebhook));
        
        config.emailEnabled = prefs.getBool("emailEnabled", false);
        prefs.getString("smtpServer", config.smtpServer, sizeof(config.smtpServer));
        config.smtpPort = prefs.getUShort("smtpPort", 587);
        prefs.getString("emailUser", config.emailUser, sizeof(config.emailUser));
        prefs.getString("emailPass", config.emailPassword, sizeof(config.emailPassword));
        prefs.getString("emailFrom", config.emailFrom, sizeof(config.emailFrom));
        prefs.getString("emailTo", config.emailTo, sizeof(config.emailTo));
        config.emailTLS = prefs.getBool("emailTLS", true);
        
        config.tempHighThreshold = prefs.getFloat("tempHigh", 30.0);
        config.tempLowThreshold = prefs.getFloat("tempLow", 10.0);
        config.batteryLowThreshold = prefs.getUChar("battLow", 20);
        config.batteryCriticalThreshold = prefs.getUChar("battCrit", 10);
        config.sensorTimeoutMinutes = prefs.getUShort("timeout", 15);
        config.rateLimitSeconds = prefs.getUInt("rateLimit", 300);
        
        config.alertTempHigh = prefs.getBool("enTempH", true);
        config.alertTempLow = prefs.getBool("enTempL", true);
        config.alertBatteryLow = prefs.getBool("enBattL", true);
        config.alertBatteryCritical = prefs.getBool("enBattC", true);
        config.alertSensorOffline = prefs.getBool("enOffline", true);
        config.alertSensorOnline = prefs.getBool("enOnline", false);
        
        prefs.end();
        Serial.println("Alert configuration loaded from NVS");
    }
}

void AlertManager::saveConfig() {
    Preferences prefs;
    if (prefs.begin("alerts", false)) {
        prefs.putBool("teamsEnabled", config.teamsEnabled);
        prefs.putString("teamsWebhook", config.teamsWebhook);
        
        prefs.putBool("emailEnabled", config.emailEnabled);
        prefs.putString("smtpServer", config.smtpServer);
        prefs.putUShort("smtpPort", config.smtpPort);
        prefs.putString("emailUser", config.emailUser);
        prefs.putString("emailPass", config.emailPassword);
        prefs.putString("emailFrom", config.emailFrom);
        prefs.putString("emailTo", config.emailTo);
        prefs.putBool("emailTLS", config.emailTLS);
        
        prefs.putFloat("tempHigh", config.tempHighThreshold);
        prefs.putFloat("tempLow", config.tempLowThreshold);
        prefs.putUChar("battLow", config.batteryLowThreshold);
        prefs.putUChar("battCrit", config.batteryCriticalThreshold);
        prefs.putUShort("timeout", config.sensorTimeoutMinutes);
        prefs.putUInt("rateLimit", config.rateLimitSeconds);
        
        prefs.putBool("enTempH", config.alertTempHigh);
        prefs.putBool("enTempL", config.alertTempLow);
        prefs.putBool("enBattL", config.alertBatteryLow);
        prefs.putBool("enBattC", config.alertBatteryCritical);
        prefs.putBool("enOffline", config.alertSensorOffline);
        prefs.putBool("enOnline", config.alertSensorOnline);
        
        prefs.end();
        Serial.println("Alert configuration saved to NVS");
    }
}

AlertConfig* AlertManager::getConfig() {
    return &config;
}

void AlertManager::setTeamsWebhook(const char* webhook) {
    strncpy(config.teamsWebhook, webhook, sizeof(config.teamsWebhook) - 1);
    config.teamsWebhook[sizeof(config.teamsWebhook) - 1] = '\0';
}

void AlertManager::setTemperatureThresholds(float low, float high) {
    config.tempLowThreshold = low;
    config.tempHighThreshold = high;
}

void AlertManager::setBatteryThresholds(uint8_t low, uint8_t critical) {
    config.batteryLowThreshold = low;
    config.batteryCriticalThreshold = critical;
}

void AlertManager::setSensorTimeout(uint16_t minutes) {
    config.sensorTimeoutMinutes = minutes;
}

void AlertManager::setRateLimit(uint32_t seconds) {
    config.rateLimitSeconds = seconds;
}

void AlertManager::enableAlert(AlertType type, bool enabled) {
    switch (type) {
        case ALERT_TEMPERATURE_HIGH:
            config.alertTempHigh = enabled;
            break;
        case ALERT_TEMPERATURE_LOW:
            config.alertTempLow = enabled;
            break;
        case ALERT_BATTERY_LOW:
            config.alertBatteryLow = enabled;
            break;
        case ALERT_BATTERY_CRITICAL:
            config.alertBatteryCritical = enabled;
            break;
        case ALERT_SENSOR_OFFLINE:
            config.alertSensorOffline = enabled;
            break;
        case ALERT_SENSOR_ONLINE:
            config.alertSensorOnline = enabled;
            break;
        default:
            break;
    }
}

bool AlertManager::shouldSendAlert(uint8_t sensorId, AlertType type) {
    if (!initialized || !config.teamsEnabled) return false;
    if (sensorId >= 20) return false;
    
    uint32_t now = millis();
    uint32_t lastTime = state.lastAlertTime[sensorId];
    AlertType lastType = state.lastAlertType[sensorId];
    
    // Always allow if it's a different alert type
    if (lastType != type) return true;
    
    // Get sensor priority to adjust rate limit
    #ifdef BASE_STATION
        extern SensorConfigManager sensorConfigManager;
        SensorPriority priority = sensorConfigManager.getSensorPriority(sensorId);
        
        uint32_t rateLimitMs;
        switch(priority) {
            case PRIORITY_HIGH:
                rateLimitMs = config.rateLimitSeconds * 500;  // 50% of standard (more frequent alerts)
                break;
            case PRIORITY_LOW:
                rateLimitMs = config.rateLimitSeconds * 4000; // 4x standard (less frequent alerts)
                break;
            case PRIORITY_MEDIUM:
            default:
                rateLimitMs = config.rateLimitSeconds * 1000; // Standard rate limit
                break;
        }
    #else
        uint32_t rateLimitMs = config.rateLimitSeconds * 1000;
    #endif
    
    // Check rate limit for same alert type
    if (now - lastTime < rateLimitMs) {
        return false;
    }
    
    return true;
}

String AlertManager::getAlertTitle(AlertType type) {
    switch (type) {
        case ALERT_TEMPERATURE_HIGH: return "⚠️ High Temperature Alert";
        case ALERT_TEMPERATURE_LOW: return "❄️ Low Temperature Alert";
        case ALERT_BATTERY_LOW: return "🔋 Low Battery Alert";
        case ALERT_BATTERY_CRITICAL: return "⚡ Critical Battery Alert";
        case ALERT_SENSOR_OFFLINE: return "📵 Sensor Offline Alert";
        case ALERT_SENSOR_ONLINE: return "✅ Sensor Back Online";
        case ALERT_COMMUNICATION_FAILURE: return "📡 Communication Failure";
        case ALERT_SYSTEM_ERROR: return "❌ System Error";
        default: return "📢 Alert";
    }
}

String AlertManager::getAlertColor(AlertType type) {
    switch (type) {
        case ALERT_TEMPERATURE_HIGH: return "FF6B35";  // Orange-red
        case ALERT_TEMPERATURE_LOW: return "4A90E2";   // Blue
        case ALERT_BATTERY_LOW: return "FFB800";       // Yellow
        case ALERT_BATTERY_CRITICAL: return "D32F2F";  // Red
        case ALERT_SENSOR_OFFLINE: return "D32F2F";    // Red
        case ALERT_SENSOR_ONLINE: return "4CAF50";     // Green
        case ALERT_COMMUNICATION_FAILURE: return "D32F2F"; // Red
        case ALERT_SYSTEM_ERROR: return "9E9E9E";      // Gray
        default: return "0078D4";  // Microsoft Blue
    }
}

String AlertManager::formatTeamsCard(const String& title, const String& message, const String& color, const String& details) {
    String json = "{";
    json += "\"@type\":\"MessageCard\",";
    json += "\"@context\":\"https://schema.org/extensions\",";
    json += "\"themeColor\":\"" + color + "\",";
    json += "\"title\":\"" + title + "\",";
    json += "\"text\":\"" + message + "\"";
    
    if (details.length() > 0) {
        json += ",\"sections\":[{\"text\":\"" + details + "\"}]";
    }
    
    json += "}";
    return json;
}

bool AlertManager::sendTeamsAlert(const String& title, const String& message, const String& color) {
    if (!config.teamsEnabled || strlen(config.teamsWebhook) == 0) {
        Serial.println("Teams alerts not enabled or webhook not configured");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - cannot send Teams alert");
        return false;
    }
    
    HTTPClient http;
    WiFiClientSecure client;
    
    // For Microsoft Teams webhooks, we need to accept their certificate
    client.setInsecure();

    // Prevent rare long stalls (DNS/TLS/connect/write/read) from blocking the main loop
    // or the AsyncWebServer callback for too long and triggering a watchdog reset.
    client.setTimeout(5); // seconds
#if defined(ARDUINO_ARCH_ESP32)
    client.setHandshakeTimeout(5); // seconds
#endif
    http.setReuse(false);
    http.setTimeout(5000); // ms
    
    http.begin(client, config.teamsWebhook);
    http.addHeader("Content-Type", "application/json");
    
    String payload = formatTeamsCard(title, message, color, "");
    
    Serial.println("Sending Teams alert...");
    Serial.println("Webhook: " + String(config.teamsWebhook));
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        Serial.printf("Teams alert sent, HTTP response: %d\n", httpCode);
        if (httpCode == 200) {
            http.end();
            return true;
        }
    } else {
        Serial.printf("Teams alert failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return false;
}

bool AlertManager::sendAlert(uint8_t sensorId, AlertType type, const String& message, const String& details) {
    if (!shouldSendAlert(sensorId, type)) {
        Serial.printf("Alert rate limited for sensor %d, type %d\n", sensorId, type);
        return false;
    }
    
    String title = getAlertTitle(type);
    String color = getAlertColor(type);
    
    bool success = false;
    
    // Send Teams alert if enabled
    if (config.teamsEnabled) {
        success = sendTeamsAlert(title, message, color);
    }
    
    // Send email alert if enabled (base station only)
#ifdef BASE_STATION
    if (config.emailEnabled) {
        String emailSubject = "LoRa Alert: " + title;
        String emailBody = message;
        if (details.length() > 0) {
            emailBody += "\n\n" + details;
        }
        bool emailSuccess = sendEmailAlert(emailSubject, emailBody);
        success = success || emailSuccess;  // Success if either method works
    }
#endif
    
    if (success && sensorId < 20) {
        state.lastAlertTime[sensorId] = millis();
        state.lastAlertType[sensorId] = type;
    }
    
    return success;
}

void AlertManager::checkSensorAlerts(uint8_t sensorId, float temperature, uint8_t battery, bool online) {
    if (!initialized || (!config.teamsEnabled && !config.emailEnabled)) return;
    
    // Check temperature thresholds
    if (config.alertTempHigh && temperature > config.tempHighThreshold) {
        String msg = "Sensor #" + String(sensorId) + " temperature is " + String(temperature, 1) + "°C";
        String details = "Threshold: " + String(config.tempHighThreshold, 1) + "°C";
        sendAlert(sensorId, ALERT_TEMPERATURE_HIGH, msg, details);
    }
    
    if (config.alertTempLow && temperature < config.tempLowThreshold) {
        String msg = "Sensor #" + String(sensorId) + " temperature is " + String(temperature, 1) + "°C";
        String details = "Threshold: " + String(config.tempLowThreshold, 1) + "°C";
        sendAlert(sensorId, ALERT_TEMPERATURE_LOW, msg, details);
    }
    
    // Check battery thresholds
    if (config.alertBatteryCritical && battery <= config.batteryCriticalThreshold) {
        String msg = "Sensor #" + String(sensorId) + " battery critically low: " + String(battery) + "%";
        String details = "Please replace or charge battery immediately!";
        sendAlert(sensorId, ALERT_BATTERY_CRITICAL, msg, details);
    } else if (config.alertBatteryLow && battery <= config.batteryLowThreshold) {
        String msg = "Sensor #" + String(sensorId) + " battery low: " + String(battery) + "%";
        String details = "Consider replacing or charging battery soon";
        sendAlert(sensorId, ALERT_BATTERY_LOW, msg, details);
    }
    
    // Check online status
    if (!online && config.alertSensorOffline) {
        String msg = "Sensor #" + String(sensorId) + " has gone offline";
        String details = "No communication for " + String(config.sensorTimeoutMinutes) + " minutes";
        sendAlert(sensorId, ALERT_SENSOR_OFFLINE, msg, details);
    } else if (online && config.alertSensorOnline) {
        // Only send "back online" if we previously sent an offline alert
        if (state.lastAlertType[sensorId] == ALERT_SENSOR_OFFLINE) {
            String msg = "Sensor #" + String(sensorId) + " is back online";
            sendAlert(sensorId, ALERT_SENSOR_ONLINE, msg, "");
        }
    }
}

void AlertManager::checkAllSensors() {
    if (!initialized || (!config.teamsEnabled && !config.emailEnabled)) return;
    
    for (uint8_t i = 0; i < 10; i++) {
        SensorInfo* sensor = getSensorByIndex(i);
        if (sensor != NULL) {
            bool online = !isSensorTimedOut(sensor->sensorId);
            checkSensorAlerts(sensor->sensorId, sensor->lastTemperature, 
                            sensor->lastBatteryPercent, online);
        }
    }
}

bool AlertManager::testTeamsWebhook() {
    String title = "🧪 Test Alert";
    String message = "This is a test alert from your LoRa Sensor Station. If you see this, your Teams webhook is configured correctly!";
    String color = "0078D4";  // Microsoft Blue
    
    return sendTeamsAlert(title, message, color);
}

#ifdef BASE_STATION
bool AlertManager::sendEmailAlert(const String& subject, const String& message) {
    if (!config.emailEnabled || strlen(config.emailUser) == 0) {
        Serial.println("Email alerts not enabled or not configured");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - cannot send email");
        return false;
    }
    
    Serial.println("Sending email alert...");
    
    // Create SMTP session
    SMTPSession smtp;
    // Avoid long stalls on unreachable SMTP servers / DNS issues.
    smtp.setTCPTimeout(5); // seconds
    
    // Set the session config
    Session_Config sessionConfig;
    sessionConfig.server.host_name = config.smtpServer;
    sessionConfig.server.port = config.smtpPort;
    sessionConfig.login.email = config.emailUser;
    sessionConfig.login.password = config.emailPassword;
    sessionConfig.login.user_domain = "";
    
    // Set the NTP config
    sessionConfig.time.ntp_server = "pool.ntp.org,time.nist.gov";
    sessionConfig.time.gmt_offset = 0;
    sessionConfig.time.day_light_offset = 0;
    
    // Declare the message class
    SMTP_Message emailMessage;
    
    // Set the message headers
    emailMessage.sender.name = "LoRa Sensor Station";
    emailMessage.sender.email = config.emailFrom;
    emailMessage.subject = subject;
    emailMessage.addRecipient("Alert Recipient", config.emailTo);
    
    // Set the message content
    String htmlMsg = "<div style='font-family: Arial, sans-serif;'>";
    htmlMsg += "<h2 style='color: #667eea;'>LoRa Sensor Station Alert</h2>";
    htmlMsg += "<p>" + message + "</p>";
    htmlMsg += "<hr style='border: 1px solid #e0e0e0;'>";
    htmlMsg += "<p style='font-size: 12px; color: #666;'>Sent from LoRa Sensor Station</p>";
    htmlMsg += "</div>";
    
    emailMessage.text.content = message.c_str();
    emailMessage.text.charSet = "us-ascii";
    emailMessage.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    
    emailMessage.html.content = htmlMsg.c_str();
    emailMessage.html.charSet = "utf-8";
    emailMessage.html.transfer_encoding = Content_Transfer_Encoding::enc_qp;
    
    emailMessage.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
    
    // Connect to server with the session config
    if (!smtp.connect(&sessionConfig)) {
        Serial.printf("Email connection failed: %s\n", smtp.errorReason().c_str());
        return false;
    }
    
    // Start sending Email
    if (!MailClient.sendMail(&smtp, &emailMessage)) {
        Serial.printf("Email send failed: %s\n", smtp.errorReason().c_str());
        smtp.closeSession();
        return false;
    }
    
    Serial.println("Email sent successfully!");
    smtp.closeSession();
    return true;
}

bool AlertManager::testEmailSettings() {
    String subject = "Test Alert from LoRa Sensor Station";
    String message = "This is a test email from your LoRa Sensor Station. If you receive this, your email settings are configured correctly!";
    
    return sendEmailAlert(subject, message);
}
#endif