#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Sensor metadata stored on base station
 */
struct SensorMetadata {
    uint8_t sensorId;
    char location[32];
    char notes[64];
    uint16_t transmitInterval;  // Seconds
    float tempThresholdMin;
    float tempThresholdMax;
    bool alertsEnabled;
    bool configured;
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

private:
    Preferences prefs;
    
    String getSensorKey(uint8_t sensorId, const char* field);
};

#endif // SENSOR_CONFIG_H
