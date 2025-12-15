#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include "config_storage.h"  // For SensorPriority enum

// Sensor health scoring
struct SensorHealthScore {
    float communicationReliability;  // 0.0-1.0 (packet success rate)
    float readingQuality;            // 0.0-1.0 (variance/outlier detection)
    float batteryHealth;             // 0.0-1.0 (degradation tracking)
    float overallHealth;             // 0.0-1.0 (weighted average)
    uint32_t uptimeSeconds;          // Time since first seen
    uint32_t lastSeenTimestamp;      // millis() of last contact
    uint16_t totalPackets;           // Total packets received
    uint16_t failedPackets;          // Failed/lost packets
};

/**
 * @brief Sensor metadata stored on base station
 */
struct SensorMetadata {
    uint8_t sensorId;
    char location[32];
    char zone[16];              // Zone/group name
    char notes[64];
    uint16_t transmitInterval;  // Seconds
    SensorPriority priority;    // Priority level
    float tempThresholdMin;
    float tempThresholdMax;
    bool alertsEnabled;
    bool configured;
    
    // Health tracking
    SensorHealthScore health;
};

/**
 * @brief Manages sensor metadata on base station
 */
class SensorConfigManager {
public:
    SensorConfigManager();
    
    bool begin();
    
    // Get sensor metadata
    SensorMetadata getSensorMetadata(uint8_t sensorId);
    
    // Set sensor metadata
    bool setSensorMetadata(uint8_t sensorId, const SensorMetadata& metadata);
    
    // Check if sensor has custom configuration
    bool hasSensorMetadata(uint8_t sensorId);
    
    // Get location string (returns configured name or default)
    String getSensorLocation(uint8_t sensorId);
    
    // List all configured sensors
    void listConfiguredSensors();
    
    // Clear sensor configuration
    bool clearSensorMetadata(uint8_t sensorId);
    
    // Health score management
    void updateHealthScore(uint8_t sensorId, bool packetSuccess, float batteryVoltage, float temperature);
    SensorHealthScore getHealthScore(uint8_t sensorId);
    
    // Zone management
    String getSensorZone(uint8_t sensorId);
    bool setSensorZone(uint8_t sensorId, const char* zone);
    
    // Priority management
    SensorPriority getSensorPriority(uint8_t sensorId);
    bool setSensorPriority(uint8_t sensorId, SensorPriority priority);

private:
    Preferences prefs;
    
    String getSensorKey(uint8_t sensorId, const char* field);
    
    // Health calculation helpers
    float calculateCommunicationReliability(uint16_t totalPackets, uint16_t failedPackets);
    float calculateReadingQuality(float currentValue, float* history, uint8_t historySize);
    float calculateBatteryHealth(float currentVoltage, uint32_t uptimeSeconds);
};

#endif // SENSOR_CONFIG_H
