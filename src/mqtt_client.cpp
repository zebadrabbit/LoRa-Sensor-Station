/**
 * @file mqtt_client.cpp
 * @brief MQTT client implementation
 * @version 2.8.0
 * @date 2025-12-13
 */

#ifdef BASE_STATION

#include "mqtt_client.h"
#include "sensor_interface.h"
#include "data_types.h"
#include <Preferences.h>
#include <ArduinoJson.h>

// Global instance
MQTTClientManager mqttClient;

/**
 * @brief Constructor - Initialize MQTT client
 */
MQTTClientManager::MQTTClientManager() : 
    mqttClient(wifiClient),
    initialized(false),
    lastConnectAttempt(0),
    reconnectDelay(MIN_RECONNECT_DELAY),
    publishCount(0),
    failedPublishCount(0),
    reconnectCount(0)
{
    // Initialize default configuration
    config.enabled = false;
    strcpy(config.broker, "");
    config.port = 1883;
    strcpy(config.username, "");
    strcpy(config.password, "");
    strcpy(config.topicPrefix, "lora");
    config.homeAssistantDiscovery = true;
    config.qos = 0;
}

/**
 * @brief Initialize MQTT client
 */
void MQTTClientManager::begin() {
    Serial.println("Initializing MQTT client...");
    loadConfig();
    
    if (config.enabled && strlen(config.broker) > 0) {
        mqttClient.setServer(config.broker, config.port);
        mqttClient.setBufferSize(512);  // Increase buffer for Home Assistant discovery
        initialized = true;
        Serial.printf("MQTT configured: %s:%d\n", config.broker, config.port);
    } else {
        Serial.println("MQTT disabled or not configured");
    }
}

/**
 * @brief Load configuration from NVS
 */
void MQTTClientManager::loadConfig() {
    Preferences prefs;
    prefs.begin("mqtt", true);  // Read-only
    
    config.enabled = prefs.getBool("enabled", false);
    prefs.getString("broker", config.broker, sizeof(config.broker));
    config.port = prefs.getUShort("port", 1883);
    prefs.getString("username", config.username, sizeof(config.username));
    prefs.getString("password", config.password, sizeof(config.password));
    prefs.getString("prefix", config.topicPrefix, sizeof(config.topicPrefix));
    config.homeAssistantDiscovery = prefs.getBool("haDiscovery", true);
    config.qos = prefs.getUChar("qos", 0);
    
    prefs.end();
    
    Serial.printf("MQTT Config loaded - Enabled: %d, Broker: %s:%d\n", 
                  config.enabled, config.broker, config.port);
}

/**
 * @brief Save configuration to NVS
 */
void MQTTClientManager::saveConfig() {
    Preferences prefs;
    prefs.begin("mqtt", false);  // Read-write
    
    prefs.putBool("enabled", config.enabled);
    prefs.putString("broker", config.broker);
    prefs.putUShort("port", config.port);
    prefs.putString("username", config.username);
    prefs.putString("password", config.password);
    prefs.putString("prefix", config.topicPrefix);
    prefs.putBool("haDiscovery", config.homeAssistantDiscovery);
    prefs.putUChar("qos", config.qos);
    
    prefs.end();
    
    Serial.println("MQTT configuration saved to NVS");
}

/**
 * @brief Get configuration pointer
 */
MQTTConfig* MQTTClientManager::getConfig() {
    return &config;
}

/**
 * @brief Connect to MQTT broker
 */
bool MQTTClientManager::connect() {
    if (!initialized || !config.enabled || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    if (mqttClient.connected()) {
        return true;
    }
    
    // Respect reconnect delay
    uint32_t now = millis();
    if (now - lastConnectAttempt < reconnectDelay) {
        return false;
    }
    
    lastConnectAttempt = now;
    Serial.printf("Connecting to MQTT broker %s:%d...\n", config.broker, config.port);
    
    // Generate unique client ID
    String clientId = "LoRaBase-" + WiFi.macAddress();
    clientId.replace(":", "");
    
    bool connected;
    if (strlen(config.username) > 0) {
        connected = mqttClient.connect(clientId.c_str(), config.username, config.password);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("MQTT connected!");
        reconnectDelay = MIN_RECONNECT_DELAY;  // Reset delay on success
        reconnectCount++;
        
        // Publish online status
        String statusTopic = buildTopic("status");
        mqttClient.publish(statusTopic.c_str(), "online", true);
        
        return true;
    } else {
        Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
        
        // Exponential backoff
        reconnectDelay = min(reconnectDelay * 2, MAX_RECONNECT_DELAY);
        
        return false;
    }
}

/**
 * @brief Disconnect from MQTT broker
 */
void MQTTClientManager::disconnect() {
    if (mqttClient.connected()) {
        String statusTopic = buildTopic("status");
        mqttClient.publish(statusTopic.c_str(), "offline", true);
        mqttClient.disconnect();
        Serial.println("MQTT disconnected");
    }
}

/**
 * @brief Check if connected to MQTT broker
 */
bool MQTTClientManager::isConnected() {
    return mqttClient.connected();
}

/**
 * @brief Maintain MQTT connection (call in loop)
 */
void MQTTClientManager::loop() {
    if (!initialized || !config.enabled) {
        return;
    }
    
    if (!mqttClient.connected()) {
        connect();
    } else {
        mqttClient.loop();
    }
}

/**
 * @brief Publish sensor data to MQTT
 */
bool MQTTClientManager::publishSensorData(uint8_t sensorId, const char* location, 
                                         float temperature, uint8_t battery, 
                                         int16_t rssi, int8_t snr) {
    if (!isConnected()) {
        failedPublishCount++;
        return false;
    }
    
    // Publish individual topics
    String tempTopic = buildSensorTopic(sensorId, "temperature");
    String battTopic = buildSensorTopic(sensorId, "battery");
    String rssiTopic = buildSensorTopic(sensorId, "rssi");
    String snrTopic = buildSensorTopic(sensorId, "snr");
    
    char payload[16];
    bool success = true;
    
    // Temperature
    snprintf(payload, sizeof(payload), "%.1f", temperature);
    success &= publish(tempTopic.c_str(), payload);
    
    // Battery
    snprintf(payload, sizeof(payload), "%d", battery);
    success &= publish(battTopic.c_str(), payload);
    
    // RSSI
    snprintf(payload, sizeof(payload), "%d", rssi);
    success &= publish(rssiTopic.c_str(), payload);
    
    // SNR
    snprintf(payload, sizeof(payload), "%d", snr);
    success &= publish(snrTopic.c_str(), payload);
    
    // Also publish combined JSON (for compatibility)
    String jsonTopic = buildSensorTopic(sensorId, "state");
    StaticJsonDocument<256> doc;
    doc["sensor_id"] = sensorId;
    doc["location"] = location;
    doc["temperature"] = temperature;
    doc["battery"] = battery;
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["timestamp"] = millis() / 1000;
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    success &= publish(jsonTopic.c_str(), jsonPayload.c_str());
    
    if (success) {
        publishCount++;
    } else {
        failedPublishCount++;
    }
    
    return success;
}

/**
 * @brief Publish multi-sensor data to MQTT (Phase 2 support)
 */
bool MQTTClientManager::publishMultiSensorData(uint8_t sensorId, const char* location,
                                              const SensorValuePacket* values, uint8_t valueCount,
                                              uint8_t battery, int16_t rssi, int8_t snr) {
    if (!isConnected()) {
        failedPublishCount++;
        return false;
    }
    
    char payload[16];
    bool success = true;
    
    // Publish each sensor value to its own topic
    for (uint8_t i = 0; i < valueCount; i++) {
        String topic;
        snprintf(payload, sizeof(payload), "%.2f", values[i].value);
        
        switch(values[i].type) {
            case VALUE_TEMPERATURE:
                topic = buildSensorTopic(sensorId, "temperature");
                break;
            case VALUE_HUMIDITY:
                topic = buildSensorTopic(sensorId, "humidity");
                break;
            case VALUE_PRESSURE:
                topic = buildSensorTopic(sensorId, "pressure");
                break;
            case VALUE_LIGHT:
                topic = buildSensorTopic(sensorId, "light");
                break;
            case VALUE_VOLTAGE:
                topic = buildSensorTopic(sensorId, "voltage");
                break;
            case VALUE_CURRENT:
                topic = buildSensorTopic(sensorId, "current");
                break;
            case VALUE_POWER:
                topic = buildSensorTopic(sensorId, "power");
                break;
            case VALUE_GAS_RESISTANCE:
                // Convert ohms to kilo-ohms for display
                snprintf(payload, sizeof(payload), "%.2f", values[i].value / 1000.0f);
                topic = buildSensorTopic(sensorId, "gas_resistance");
                break;
            case VALUE_MOISTURE:
                topic = buildSensorTopic(sensorId, "moisture");
                break;
            default:
                // Skip unknown value types
                continue;
        }
        
        success &= publish(topic.c_str(), payload);
    }
    
    // Publish battery and signal strength
    snprintf(payload, sizeof(payload), "%d", battery);
    success &= publish(buildSensorTopic(sensorId, "battery").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%d", rssi);
    success &= publish(buildSensorTopic(sensorId, "rssi").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%d", snr);
    success &= publish(buildSensorTopic(sensorId, "snr").c_str(), payload);
    
    // Publish combined JSON state
    String jsonTopic = buildSensorTopic(sensorId, "state");
    StaticJsonDocument<512> doc;
    doc["sensor_id"] = sensorId;
    doc["location"] = location;
    doc["battery"] = battery;
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["timestamp"] = millis() / 1000;
    
    // Add all sensor values to JSON
    JsonObject readings = doc.createNestedObject("readings");
    for (uint8_t i = 0; i < valueCount; i++) {
        switch(values[i].type) {
            case VALUE_TEMPERATURE:
                readings["temperature"] = values[i].value;
                break;
            case VALUE_HUMIDITY:
                readings["humidity"] = values[i].value;
                break;
            case VALUE_PRESSURE:
                readings["pressure"] = values[i].value;
                break;
            case VALUE_LIGHT:
                readings["light"] = values[i].value;
                break;
            case VALUE_VOLTAGE:
                readings["voltage"] = values[i].value;
                break;
            case VALUE_CURRENT:
                readings["current"] = values[i].value;
                break;
            case VALUE_POWER:
                readings["power"] = values[i].value;
                break;
            case VALUE_GAS_RESISTANCE:
                readings["gas_resistance"] = values[i].value / 1000.0f;  // kΩ
                break;
            case VALUE_MOISTURE:
                readings["moisture"] = values[i].value;
                break;
        }
    }
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    success &= publish(jsonTopic.c_str(), jsonPayload.c_str());
    
    if (success) {
        publishCount++;
    } else {
        failedPublishCount++;
    }
    
    return success;
}

/**
 * @brief Publish base station status
 */
bool MQTTClientManager::publishBaseStationStatus(uint8_t activeSensors, 
                                                 uint32_t totalPackets, 
                                                 uint32_t uptime) {
    if (!isConnected()) {
        return false;
    }
    
    String topic = buildTopic("base/status");
    
    StaticJsonDocument<256> doc;
    doc["active_sensors"] = activeSensors;
    doc["total_packets"] = totalPackets;
    doc["uptime_seconds"] = uptime;
    doc["mqtt_publishes"] = publishCount;
    doc["mqtt_failures"] = failedPublishCount;
    doc["mqtt_reconnects"] = reconnectCount;
    
    String payload;
    serializeJson(doc, payload);
    
    return publish(topic.c_str(), payload.c_str());
}

/**
 * @brief Publish Home Assistant MQTT discovery config
 */
void MQTTClientManager::publishHomeAssistantDiscovery(uint8_t sensorId, const char* location) {
    if (!isConnected() || !config.homeAssistantDiscovery) {
        return;
    }
    
    String deviceName = String(location);
    if (deviceName.length() == 0) {
        deviceName = "LoRa Sensor " + String(sensorId);
    }
    
    String deviceId = "lora_sensor_" + String(sensorId);
    
    // Temperature sensor
    String tempConfigTopic = "homeassistant/sensor/" + deviceId + "_temperature/config";
    StaticJsonDocument<512> tempDoc;
    tempDoc["name"] = deviceName + " Temperature";
    tempDoc["unique_id"] = deviceId + "_temp";
    tempDoc["state_topic"] = buildSensorTopic(sensorId, "temperature");
    tempDoc["unit_of_measurement"] = "°C";
    tempDoc["device_class"] = "temperature";
    tempDoc["value_template"] = "{{ value }}";
    
    JsonObject device = tempDoc.createNestedObject("device");
    device["identifiers"][0] = deviceId;
    device["name"] = deviceName;
    device["model"] = "LoRa Sensor";
    device["manufacturer"] = "Heltec";
    
    String tempPayload;
    serializeJson(tempDoc, tempPayload);
    publish(tempConfigTopic.c_str(), tempPayload.c_str(), true);
    
    // Battery sensor
    String battConfigTopic = "homeassistant/sensor/" + deviceId + "_battery/config";
    StaticJsonDocument<512> battDoc;
    battDoc["name"] = deviceName + " Battery";
    battDoc["unique_id"] = deviceId + "_battery";
    battDoc["state_topic"] = buildSensorTopic(sensorId, "battery");
    battDoc["unit_of_measurement"] = "%";
    battDoc["device_class"] = "battery";
    battDoc["value_template"] = "{{ value }}";
    battDoc["device"] = device;
    
    String battPayload;
    serializeJson(battDoc, battPayload);
    publish(battConfigTopic.c_str(), battPayload.c_str(), true);
    
    // RSSI sensor
    String rssiConfigTopic = "homeassistant/sensor/" + deviceId + "_rssi/config";
    StaticJsonDocument<512> rssiDoc;
    rssiDoc["name"] = deviceName + " RSSI";
    rssiDoc["unique_id"] = deviceId + "_rssi";
    rssiDoc["state_topic"] = buildSensorTopic(sensorId, "rssi");
    rssiDoc["unit_of_measurement"] = "dBm";
    rssiDoc["device_class"] = "signal_strength";
    rssiDoc["value_template"] = "{{ value }}";
    rssiDoc["device"] = device;
    
    String rssiPayload;
    serializeJson(rssiDoc, rssiPayload);
    publish(rssiConfigTopic.c_str(), rssiPayload.c_str(), true);
    
    Serial.printf("Published Home Assistant discovery for sensor %d\n", sensorId);
}

/**
 * @brief Publish Home Assistant MQTT discovery config for multi-sensor device
 */
void MQTTClientManager::publishHomeAssistantMultiSensorDiscovery(uint8_t sensorId, const char* location,
                                                                const SensorValuePacket* values, 
                                                                uint8_t valueCount) {
    if (!isConnected() || !config.homeAssistantDiscovery) {
        return;
    }
    
    String deviceName = String(location);
    if (deviceName.length() == 0) {
        deviceName = "LoRa Sensor " + String(sensorId);
    }
    
    String deviceId = "lora_sensor_" + String(sensorId);
    
    // Create device info object once
    StaticJsonDocument<256> deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    device["identifiers"][0] = deviceId;
    device["name"] = deviceName;
    device["model"] = "LoRa Multi-Sensor";
    device["manufacturer"] = "Heltec";
    
    // Publish discovery for each sensor type
    for (uint8_t i = 0; i < valueCount; i++) {
        StaticJsonDocument<512> doc;
        String configTopic;
        String suffix;
        String unit;
        String deviceClass;
        String sensorName;
        
        switch(values[i].type) {
            case VALUE_TEMPERATURE:
                suffix = "temperature";
                unit = "°C";
                deviceClass = "temperature";
                sensorName = "Temperature";
                break;
            case VALUE_HUMIDITY:
                suffix = "humidity";
                unit = "%";
                deviceClass = "humidity";
                sensorName = "Humidity";
                break;
            case VALUE_PRESSURE:
                suffix = "pressure";
                unit = "hPa";
                deviceClass = "pressure";
                sensorName = "Pressure";
                break;
            case VALUE_LIGHT:
                suffix = "light";
                unit = "lx";
                deviceClass = "illuminance";
                sensorName = "Light";
                break;
            case VALUE_VOLTAGE:
                suffix = "voltage";
                unit = "V";
                deviceClass = "voltage";
                sensorName = "Voltage";
                break;
            case VALUE_CURRENT:
                suffix = "current";
                unit = "mA";
                deviceClass = "current";
                sensorName = "Current";
                break;
            case VALUE_POWER:
                suffix = "power";
                unit = "mW";
                deviceClass = "power";
                sensorName = "Power";
                break;
            case VALUE_GAS_RESISTANCE:
                suffix = "gas_resistance";
                unit = "kΩ";
                deviceClass = "";  // No standard device class
                sensorName = "Gas Resistance";
                break;
            case VALUE_MOISTURE:
                suffix = "moisture";
                unit = "%";
                deviceClass = "moisture";
                sensorName = "Moisture";
                break;
            default:
                // Skip unknown types
                continue;
        }
        
        configTopic = "homeassistant/sensor/" + deviceId + "_" + suffix + "/config";
        
        doc["name"] = deviceName + " " + sensorName;
        doc["unique_id"] = deviceId + "_" + suffix;
        doc["state_topic"] = buildSensorTopic(sensorId, suffix.c_str());
        doc["unit_of_measurement"] = unit;
        if (deviceClass.length() > 0) {
            doc["device_class"] = deviceClass;
        }
        doc["value_template"] = "{{ value }}";
        doc["device"] = device;
        
        String payload;
        serializeJson(doc, payload);
        publish(configTopic.c_str(), payload.c_str(), true);
    }
    
    // Also publish battery and RSSI
    StaticJsonDocument<512> battDoc;
    battDoc["name"] = deviceName + " Battery";
    battDoc["unique_id"] = deviceId + "_battery";
    battDoc["state_topic"] = buildSensorTopic(sensorId, "battery");
    battDoc["unit_of_measurement"] = "%";
    battDoc["device_class"] = "battery";
    battDoc["value_template"] = "{{ value }}";
    battDoc["device"] = device;
    
    String battPayload;
    serializeJson(battDoc, battPayload);
    String battConfigTopic = "homeassistant/sensor/" + deviceId + "_battery/config";
    publish(battConfigTopic.c_str(), battPayload.c_str(), true);
    
    StaticJsonDocument<512> rssiDoc;
    rssiDoc["name"] = deviceName + " RSSI";
    rssiDoc["unique_id"] = deviceId + "_rssi";
    rssiDoc["state_topic"] = buildSensorTopic(sensorId, "rssi");
    rssiDoc["unit_of_measurement"] = "dBm";
    rssiDoc["device_class"] = "signal_strength";
    rssiDoc["value_template"] = "{{ value }}";
    rssiDoc["device"] = device;
    
    String rssiPayload;
    serializeJson(rssiDoc, rssiPayload);
    String rssiConfigTopic = "homeassistant/sensor/" + deviceId + "_rssi/config";
    publish(rssiConfigTopic.c_str(), rssiPayload.c_str(), true);
    
    Serial.printf("Published Home Assistant multi-sensor discovery for sensor %d (%d types)\n", 
                  sensorId, valueCount);
}

/**
 * @brief Remove Home Assistant discovery config
 */
void MQTTClientManager::removeHomeAssistantDiscovery(uint8_t sensorId) {
    if (!isConnected()) {
        return;
    }
    
    String deviceId = "lora_sensor_" + String(sensorId);
    
    String tempConfigTopic = "homeassistant/sensor/" + deviceId + "_temperature/config";
    String battConfigTopic = "homeassistant/sensor/" + deviceId + "_battery/config";
    String rssiConfigTopic = "homeassistant/sensor/" + deviceId + "_rssi/config";
    
    publish(tempConfigTopic.c_str(), "", true);
    publish(battConfigTopic.c_str(), "", true);
    publish(rssiConfigTopic.c_str(), "", true);
}

/**
 * @brief Get publish count
 */
uint32_t MQTTClientManager::getPublishCount() {
    return publishCount;
}

/**
 * @brief Get failed publish count
 */
uint32_t MQTTClientManager::getFailedPublishCount() {
    return failedPublishCount;
}

/**
 * @brief Get reconnect count
 */
uint32_t MQTTClientManager::getReconnectCount() {
    return reconnectCount;
}

/**
 * @brief Publish message to topic
 */
bool MQTTClientManager::publish(const char* topic, const char* payload, bool retain) {
    if (!isConnected()) {
        return false;
    }
    
    bool result = mqttClient.publish(topic, payload, retain);
    if (!result) {
        Serial.printf("MQTT publish failed to %s\n", topic);
    }
    return result;
}

/**
 * @brief Build topic with prefix
 */
String MQTTClientManager::buildTopic(const char* suffix) {
    return String(config.topicPrefix) + "/" + String(suffix);
}

/**
 * @brief Build sensor-specific topic
 */
String MQTTClientManager::buildSensorTopic(uint8_t sensorId, const char* suffix) {
    return String(config.topicPrefix) + "/sensor/" + String(sensorId) + "/" + String(suffix);
}

#endif // BASE_STATION
