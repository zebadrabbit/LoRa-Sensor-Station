#include "statistics.h"
#include "config.h"
#include <Arduino.h>
#ifdef BASE_STATION
#include "sensor_config.h"
extern SensorConfigManager sensorConfigManager;
#endif

#define MAX_CLIENTS 10

// Reuse MAX_SENSORS from config.h, but expand it for physical sensor storage
#undef MAX_SENSORS
#define MAX_SENSORS 40  // Max 10 clients * 4 sensors each (adjustable)

static SystemStats stats;
static ClientInfo clients[MAX_CLIENTS];
static PhysicalSensor sensors[MAX_SENSORS];

void initStats() {
  memset(&stats, 0, sizeof(SystemStats));
  memset(clients, 0, sizeof(clients));
  memset(sensors, 0, sizeof(sensors));
  
  // Initialize RSSI history with mid-range values
  for (int i = 0; i < 32; i++) {
    stats.rssiHistory[i] = -100;
  }
}

void recordTxAttempt() {
  stats.totalTxAttempts++;
}

void recordTxSuccess() {
  stats.totalTxSuccess++;
  stats.lastTxTime = millis();
}

void recordTxFailure() {
  stats.totalTxFailed++;
}

void recordRxPacket(int16_t rssi) {
  stats.totalRxPackets++;
  stats.lastRxTime = millis();
  
  // Add to ring buffer for graphing
  stats.rssiHistory[stats.rssiHistoryIndex] = rssi;
  stats.rssiHistoryIndex = (stats.rssiHistoryIndex + 1) % 32;
}

void recordRxInvalid() {
  stats.totalRxInvalid++;
}

// ============================================================================
// CLIENT TRACKING
// ============================================================================

void updateClientInfo(uint8_t clientId, uint8_t batteryPercent, bool powerState, int16_t rssi, int8_t snr) {
  ClientInfo* client = NULL;
  
  // Look for existing client
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active && clients[i].clientId == clientId) {
      client = &clients[i];
      break;
    }
  }
  
  // If not found, find empty slot
  if (client == NULL) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients[i].active) {
        client = &clients[i];
        client->clientId = clientId;
        
        // Try to get configured location name
        #ifdef BASE_STATION
        String configuredName = sensorConfigManager.getSensorLocation(clientId);
        snprintf(client->location, sizeof(client->location), "%s", configuredName.c_str());
        #else
        snprintf(client->location, sizeof(client->location), "Client %d", clientId);
        #endif
        
        client->active = true;
        break;
      }
    }
  } else {
    // Update location if configured name changed
    #ifdef BASE_STATION
    String configuredName = sensorConfigManager.getSensorLocation(clientId);
    snprintf(client->location, sizeof(client->location), "%s", configuredName.c_str());
    #endif
  }
  
  if (client != NULL) {
    client->lastSeen = millis();
    client->lastRssi = rssi;
    client->lastSnr = snr;
    client->packetsReceived++;
    client->lastBatteryPercent = batteryPercent;
    client->powerState = powerState;
    
    // Store client telemetry history
    uint8_t idx = client->history.index;
    client->history.data[idx].timestamp = millis() / 1000;
    client->history.data[idx].battery = batteryPercent;
    client->history.data[idx].rssi = rssi;
    client->history.data[idx].charging = powerState;
    
    Serial.printf("📊 CLIENT HISTORY: Client %d stored at idx %d (count=%d): batt=%d%%, rssi=%d dBm, charging=%s\n",
                  clientId, idx, client->history.count, 
                  batteryPercent, rssi, powerState ? "YES" : "NO");
    
    client->history.index = (idx + 1) % HISTORY_SIZE;
    if (client->history.count < HISTORY_SIZE) {
      client->history.count++;
    }
  }
}

uint8_t getActiveClientCount() {
  uint8_t count = 0;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active) {
      count++;
    }
  }
  return count;
}

ClientInfo* getClientInfo(uint8_t clientId) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active && clients[i].clientId == clientId) {
      return &clients[i];
    }
  }
  return NULL;
}

ClientInfo* getClientByIndex(uint8_t index) {
  if (index < MAX_CLIENTS && clients[index].active) {
    return &clients[index];
  }
  return NULL;
}

ClientInfo* getAllClients() {
  return clients;
}

void checkClientTimeouts() {
  uint32_t currentTime = millis();
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active) {
      uint32_t ageSeconds = (currentTime - clients[i].lastSeen) / 1000;
      if (ageSeconds > 600) {  // 10 minutes timeout
        clients[i].active = false;
      }
    }
  }
}

bool isClientTimedOut(uint8_t clientId) {
  ClientInfo* client = getClientInfo(clientId);
  if (client == NULL) return true;
  
  uint32_t ageSeconds = (millis() - client->lastSeen) / 1000;
  return ageSeconds > 600;
}

void setClientLocation(uint8_t clientId, const char* location) {
  ClientInfo* client = getClientInfo(clientId);
  if (client != NULL && location != NULL) {
    strncpy(client->location, location, sizeof(client->location) - 1);
    client->location[sizeof(client->location) - 1] = '\0';
  }
}

const char* getClientLocation(uint8_t clientId) {
  ClientInfo* client = getClientInfo(clientId);
  if (client != NULL) {
    return client->location;
  }
  return "Unknown";
}

ClientHistory* getClientHistory(uint8_t clientId) {
  ClientInfo* client = getClientInfo(clientId);
  if (client != NULL) {
    return &client->history;
  }
  return NULL;
}

// ============================================================================
// SENSOR TRACKING
// ============================================================================

void updateSensorReading(uint8_t clientId, uint8_t sensorIndex, uint8_t type, float value) {
  PhysicalSensor* sensor = NULL;
  
  // Look for existing sensor
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active && 
        sensors[i].clientId == clientId && 
        sensors[i].sensorIndex == sensorIndex) {
      sensor = &sensors[i];
      break;
    }
  }
  
  // If not found, find empty slot
  if (sensor == NULL) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (!sensors[i].active) {
        sensor = &sensors[i];
        sensor->clientId = clientId;
        sensor->sensorIndex = sensorIndex;
        sensor->type = type;
        sensor->active = true;
        break;
      }
    }
  }
  
  if (sensor != NULL) {
    sensor->lastSeen = millis();
    sensor->lastValue = value;
    sensor->type = type;  // Update type in case it changed
    
    // Store sensor reading history
    uint8_t idx = sensor->history.index;
    sensor->history.data[idx].timestamp = millis() / 1000;
    sensor->history.data[idx].value = value;
    
    Serial.printf("📊 SENSOR HISTORY: Client %d Sensor %d stored at idx %d (count=%d): type=%d, value=%.2f\n",
                  clientId, sensorIndex, idx, sensor->history.count, type, value);
    
    sensor->history.index = (idx + 1) % HISTORY_SIZE;
    if (sensor->history.count < HISTORY_SIZE) {
      sensor->history.count++;
    }
  }
}

uint8_t getActiveSensorCount() {
  uint8_t count = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active) {
      count++;
    }
  }
  return count;
}

PhysicalSensor* getSensor(uint8_t clientId, uint8_t sensorIndex) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active && 
        sensors[i].clientId == clientId && 
        sensors[i].sensorIndex == sensorIndex) {
      return &sensors[i];
    }
  }
  return NULL;
}

PhysicalSensor* getSensorByGlobalIndex(uint8_t index) {
  if (index < MAX_SENSORS && sensors[index].active) {
    return &sensors[index];
  }
  return NULL;
}

PhysicalSensor* getAllPhysicalSensors() {
  return sensors;
}

void checkSensorTimeouts() {
  uint32_t currentTime = millis();
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active) {
      uint32_t ageSeconds = (currentTime - sensors[i].lastSeen) / 1000;
      if (ageSeconds > 600) {  // 10 minutes timeout
        sensors[i].active = false;
      }
    }
  }
}

SensorHistory* getSensorHistory(uint8_t clientId, uint8_t sensorIndex) {
  PhysicalSensor* sensor = getSensor(clientId, sensorIndex);
  if (sensor != NULL) {
    return &sensor->history;
  }
  return NULL;
}

// ============================================================================
// LEGACY COMPATIBILITY
// ============================================================================

// Legacy function for backward compatibility with existing code
// This bridges the old SensorData structure to the new client/sensor separation
void updateSensorInfo(const SensorData& data, int16_t rssi, int8_t snr) {
  // Update client telemetry
  updateClientInfo(data.sensorId, data.batteryPercent, data.powerState, rssi, snr);
  
  // Update legacy compatibility fields
  ClientInfo* client = getClientInfo(data.sensorId);
  if (client != NULL) {
    client->sensorId = data.sensorId;  // Populate legacy alias
    client->lastTemperature = data.temperature;  // Populate legacy field
  }
  
  // If there's a valid temperature reading, update sensor
  if (data.temperature > -127.0f) {
    updateSensorReading(data.sensorId, 0, VALUE_TEMPERATURE, data.temperature);
  }
}

SystemStats* getStats() {
  return &stats;
}
