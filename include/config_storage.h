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

// Client type enumeration
enum ClientType {
    CLIENT_STANDARD = 0,      // Standard client (AC power + battery backup)
    CLIENT_RUGGED = 1,        // Rugged outdoor client (solar + battery)
    CLIENT_DEEPSLEEP = 2      // Deep sleep client (ultra low power)
};

// Sensor priority levels
enum SensorPriority {
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM = 1,
    PRIORITY_HIGH = 2
};

// Sensor configuration structure
struct SensorConfig {
    uint8_t sensorId;
    char location[32];
    char zone[16];              // Zone/group name (e.g., "Living Room", "Outdoor")
    uint16_t transmitInterval;  // seconds
    uint16_t networkId;         // Network ID for pairing (0-65535)
    SensorPriority priority;    // Priority level (affects polling, alerts)
    ClientType clientType;      // Client type (standard/rugged/deepsleep)
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
