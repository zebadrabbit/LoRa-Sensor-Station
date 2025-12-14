#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

// Device mode enumeration
enum DeviceMode {
    MODE_UNCONFIGURED = 0,
    MODE_SENSOR = 1,
    MODE_BASE_STATION = 2
};

// Sensor configuration structure
struct SensorConfig {
    uint8_t sensorId;
    char location[32];
    uint16_t transmitInterval;  // seconds
    uint16_t networkId;         // Network ID for pairing (0-65535)
    bool meshEnabled;           // Enable mesh networking
    bool meshForwarding;        // Allow packet forwarding (relay mode)
    bool configured;
};

// Base station WiFi configuration
struct BaseStationConfig {
    char ssid[32];
    char password[64];
    uint16_t networkId;         // Network ID for pairing (0-65535)
    bool meshEnabled;           // Enable mesh networking
    bool configured;
};

// Configuration storage class
class ConfigStorage {
public:
    ConfigStorage();
    
    // Initialize NVS
    void begin();
    
    // Device mode
    DeviceMode getDeviceMode();
    void setDeviceMode(DeviceMode mode);
    
    // Sensor configuration
    SensorConfig getSensorConfig();
    void setSensorConfig(const SensorConfig& config);
    
    // Base station configuration
    BaseStationConfig getBaseStationConfig();
    void setBaseStationConfig(const BaseStationConfig& config);
    
    // Factory reset
    void clearAll();
    
    // Check if first boot (unconfigured)
    bool isFirstBoot();
    
private:
    Preferences prefs;
};

// Global instance
extern ConfigStorage configStorage;

#endif // CONFIG_STORAGE_H
