/**
 * @file sensor_manager.h
 * @brief Manages multiple sensors with auto-detection and unified data access
 * 
 * @version 2.9.0
 * @date 2025-12-13
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include "sensor_interface.h"

/**
 * @brief Sensor configuration stored in NVS (Phase 6)
 */
struct SensorSettings {
    bool enabled;
    SensorType type;
    InterfaceType interface;
    uint8_t address;        // GPIO pin or I2C address
    char name[32];
    bool publishMQTT;
    bool showOnDisplay;
    float calibrationOffset;
    float calibrationScale;
};

/**
 * @brief Manages all connected sensors
 */
class SensorManager {
private:
    std::vector<ISensor*> sensors;
    bool i2cInitialized;
    uint32_t lastScanTime;
    uint32_t scanInterval;  // Auto-scan interval (ms)
    
public:
    SensorManager();
    ~SensorManager();
    
    // Initialization
    void begin();
    void initI2C(uint8_t sda = 41, uint8_t scl = 42);
    
    // Sensor discovery
    void scanI2C();                          // Detect I2C devices
    void scanOneWire(uint8_t pin);           // Find DS18B20 sensors
    void addSensor(ISensor* sensor);         // Manually add sensor
    void removeSensor(uint8_t index);
    void clearAll();
    
    // Data operations
    bool readAll();                          // Read all sensors
    std::vector<SensorValue> getAllValues(); // Get all sensor values
    void printStatus();                      // Serial debug output
    
    // Sensor access
    ISensor* getSensor(uint8_t index);
    uint8_t getSensorCount() const;
    
    // Configuration
    bool loadConfig();
    bool saveConfig();
    bool enableSensor(uint8_t index, bool enabled);
    bool renameSensor(uint8_t index, const char* name);
    
    // Auto-scanning
    void setAutoScanInterval(uint32_t ms);
    void autoScan();  // Call periodically from loop()
    
    // Get all values as JSON string
    String toJSON();
    
    // Get specific sensor values
    bool getSensorValue(uint8_t sensorIndex, uint8_t valueIndex, SensorValue& value);
    
private:
    ISensor* createSensorFromI2C(uint8_t address);
    bool isI2CAddressInUse(uint8_t address);
};

#endif // SENSOR_MANAGER_H
