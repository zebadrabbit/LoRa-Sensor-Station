/**
 * @file mqtt_client.h
 * @brief MQTT client for publishing sensor data
 * @version 2.8.0
 * @date 2025-12-13
 * 
 * Features:
 * - MQTT broker connection with authentication
 * - Auto-reconnect on connection loss
 * - Publish sensor data (temperature, battery, RSSI)
 * - Home Assistant MQTT auto-discovery
 * - Configurable topic structure
 * - QoS support
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// MQTT configuration structure
struct MQTTConfig {
    bool enabled;
    char broker[64];
    uint16_t port;
    char username[32];
    char password[64];
    char topicPrefix[32];
    bool homeAssistantDiscovery;
    uint8_t qos;  // 0, 1, or 2
};

class MQTTClientManager {
public:
    MQTTClientManager();
    
    // Initialization and configuration
    void begin();
    void loadConfig();
    void saveConfig();
    MQTTConfig* getConfig();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected();
    void loop();  // Call regularly to maintain connection
    
    // Publishing
    bool publishSensorData(uint8_t sensorId, const char* location, float temperature, 
                          uint8_t battery, int16_t rssi, int8_t snr);
    bool publishMultiSensorData(uint8_t sensorId, const char* location,
                               const struct SensorValuePacket* values, uint8_t valueCount,
                               uint8_t battery, int16_t rssi, int8_t snr);
    bool publishBaseStationStatus(uint8_t activeSensors, uint32_t totalPackets, 
                                  uint32_t uptime);
    
    // Home Assistant Discovery
    void publishHomeAssistantDiscovery(uint8_t sensorId, const char* location);
    void publishHomeAssistantMultiSensorDiscovery(uint8_t sensorId, const char* location,
                                                 const struct SensorValuePacket* values, 
                                                 uint8_t valueCount);
    void removeHomeAssistantDiscovery(uint8_t sensorId);
    
    // Statistics
    uint32_t getPublishCount();
    uint32_t getFailedPublishCount();
    uint32_t getReconnectCount();
    
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    MQTTConfig config;
    bool initialized;
    
    // Connection state
    uint32_t lastConnectAttempt;
    uint32_t reconnectDelay;
    static const uint32_t MIN_RECONNECT_DELAY = 5000;    // 5 seconds
    static const uint32_t MAX_RECONNECT_DELAY = 300000;  // 5 minutes
    
    // Statistics
    uint32_t publishCount;
    uint32_t failedPublishCount;
    uint32_t reconnectCount;
    
    // Internal helpers
    bool publish(const char* topic, const char* payload, bool retain = false);
    String buildTopic(const char* suffix);
    String buildSensorTopic(uint8_t sensorId, const char* suffix);
    void reconnect();
};

extern MQTTClientManager mqttClient;

#endif // MQTT_CLIENT_H
