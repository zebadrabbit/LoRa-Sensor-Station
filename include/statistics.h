#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>
#include "data_types.h"

// Historical data storage size
#define HISTORY_SIZE 100  // Store last 100 readings

// ============================================================================
// CLIENT (Physical Device) Data Structures
// ============================================================================

// Client telemetry history point (battery, RSSI, charging state)
struct ClientDataPoint {
  uint32_t timestamp;
  uint8_t battery;
  int16_t rssi;
  bool charging;
};

// Client telemetry history (ring buffer)
struct ClientHistory {
  ClientDataPoint data[HISTORY_SIZE];
  uint8_t index;
  uint8_t count;
};

// Client information (physical device with radio and battery)
struct ClientInfo {
  uint8_t clientId;  // Physical device ID
  char location[32];  // Device location/name
  uint32_t lastSeen;
  int16_t lastRssi;
  int8_t lastSnr;
  uint32_t packetsReceived;
  uint8_t lastBatteryPercent;
  bool powerState;  // true = charging, false = discharging
  bool active;
  ClientHistory history;
  
  // Legacy compatibility fields (deprecated - for old code that expects these)
  uint8_t sensorId;  // Alias for clientId
  float lastTemperature;  // Primary sensor reading (deprecated)
};

// ============================================================================
// SENSOR (Measurement Device) Data Structures
// ============================================================================

// Sensor reading history point
struct SensorDataPoint {
  uint32_t timestamp;
  float value;
};

// Sensor reading history (ring buffer)
struct SensorHistory {
  SensorDataPoint data[HISTORY_SIZE];
  uint8_t index;
  uint8_t count;
};

// Physical sensor attached to a client
struct PhysicalSensor {
  uint8_t clientId;  // Which client this sensor is attached to
  uint8_t sensorIndex;  // Index on that client (0-15)
  uint8_t type;  // VALUE_TEMPERATURE, VALUE_HUMIDITY, etc.
  float lastValue;
  uint32_t lastSeen;
  bool active;
  SensorHistory history;
};

// ============================================================================
// SYSTEM STATISTICS
// ============================================================================

// Statistics tracking
struct SystemStats {
  uint32_t totalTxAttempts;
  uint32_t totalTxSuccess;
  uint32_t totalTxFailed;
  uint32_t totalRxPackets;
  uint32_t totalRxInvalid;
  uint32_t lastTxTime;
  uint32_t lastRxTime;
  int16_t rssiHistory[32];  // Ring buffer for signal graph
  uint8_t rssiHistoryIndex;
};

// ============================================================================
// API FUNCTIONS
// ============================================================================

// Initialize statistics
void initStats();

// Update statistics
void recordTxAttempt();
void recordTxSuccess();
void recordTxFailure();
void recordRxPacket(int16_t rssi);
void recordRxInvalid();

// Client tracking (base station only)
void updateClientInfo(uint8_t clientId, uint8_t batteryPercent, bool powerState, int16_t rssi, int8_t snr);
uint8_t getActiveClientCount();
ClientInfo* getClientInfo(uint8_t clientId);
ClientInfo* getClientByIndex(uint8_t index);
ClientInfo* getAllClients();
void checkClientTimeouts();
bool isClientTimedOut(uint8_t clientId);
void setClientLocation(uint8_t clientId, const char* location);
const char* getClientLocation(uint8_t clientId);
ClientHistory* getClientHistory(uint8_t clientId);

// Sensor tracking (base station only)
void updateSensorReading(uint8_t clientId, uint8_t sensorIndex, uint8_t type, float value);
uint8_t getActiveSensorCount();
PhysicalSensor* getSensor(uint8_t clientId, uint8_t sensorIndex);
PhysicalSensor* getSensorByGlobalIndex(uint8_t index);
PhysicalSensor* getAllPhysicalSensors();  // Renamed to avoid collision
void checkSensorTimeouts();
SensorHistory* getSensorHistory(uint8_t clientId, uint8_t sensorIndex);

// Get statistics
SystemStats* getStats();

// Legacy compatibility (deprecated)
void updateSensorInfo(const SensorData& data, int16_t rssi, int8_t snr);

// Legacy type aliases for backward compatibility
typedef ClientInfo SensorInfo;  // Old code treats "sensor" as the whole client device
typedef ClientHistory DataPointHistory;  // Old history structure

// Legacy data point structure (mixed client+sensor data)
struct DataPoint {
  uint32_t timestamp;
  float temperature;
  uint8_t battery;
  int16_t rssi;
};

// Legacy accessor functions (bridge to new architecture)
inline uint8_t getActiveClientCount_Legacy() { return getActiveClientCount(); }
inline SensorInfo* getSensorInfo(uint8_t clientId) { return (SensorInfo*)getClientInfo(clientId); }
inline SensorInfo* getSensorByIndex(uint8_t index) { return (SensorInfo*)getClientByIndex(index); }
inline SensorInfo* getAllSensors() { return (SensorInfo*)getAllClients(); }
inline bool isSensorTimedOut(uint8_t clientId) { return isClientTimedOut(clientId); }
inline void setSensorLocation(uint8_t clientId, const char* location) { setClientLocation(clientId, location); }
inline const char* getSensorLocation(uint8_t clientId) { return getClientLocation(clientId); }

// Note: getSensorHistory signature changed - old code using single param needs updating
inline ClientHistory* getSensorHistory(uint8_t clientId) { return getClientHistory(clientId); }

#endif // STATISTICS_H
