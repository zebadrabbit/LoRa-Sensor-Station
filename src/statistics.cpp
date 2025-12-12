#include "statistics.h"
#include "config.h"
#include <Arduino.h>

static SystemStats stats;
static SensorInfo sensors[MAX_SENSORS];

void initStats() {
  memset(&stats, 0, sizeof(SystemStats));
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

void updateSensorInfo(const SensorData& data, int16_t rssi, int8_t snr) {
  // Find or create sensor entry
  SensorInfo* sensor = NULL;
  
  // Look for existing sensor
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active && sensors[i].sensorId == data.sensorId) {
      sensor = &sensors[i];
      break;
    }
  }
  
  // If not found, find empty slot
  if (sensor == NULL) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (!sensors[i].active) {
        sensor = &sensors[i];
        sensor->sensorId = data.sensorId;
        sensor->active = true;
        break;
      }
    }
  }
  
  if (sensor != NULL) {
    sensor->lastSeen = millis();
    sensor->lastRssi = rssi;
    sensor->lastSnr = snr;
    sensor->packetsReceived++;
    sensor->lastTemperature = data.temperature;
    sensor->lastBatteryPercent = data.batteryPercent;
  }
}

uint8_t getActiveSensorCount() {
  uint8_t count = 0;
  uint32_t now = millis();
  
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active) {
      // Consider sensor inactive if not seen for 10 minutes
      if (now - sensors[i].lastSeen < 600000) {
        count++;
      }
    }
  }
  return count;
}

SensorInfo* getSensorInfo(uint8_t sensorId) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active && sensors[i].sensorId == sensorId) {
      return &sensors[i];
    }
  }
  return NULL;
}

SensorInfo* getSensorByIndex(uint8_t index) {
  uint8_t activeIndex = 0;
  uint32_t now = millis();
  
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active && (now - sensors[i].lastSeen < 600000)) {
      if (activeIndex == index) {
        return &sensors[i];
      }
      activeIndex++;
    }
  }
  return NULL;
}

SystemStats* getStats() {
  return &stats;
}

void checkSensorTimeouts() {
  uint32_t now = millis();
  
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].active) {
      uint32_t timeSinceLastSeen = now - sensors[i].lastSeen;
      
      // Sensor timeout threshold: 3x the longest transmit interval (300s * 3 = 900s = 15 minutes)
      if (timeSinceLastSeen > 900000) {
        Serial.printf("WARNING: Sensor #%d has timed out (last seen %lu seconds ago)\n", 
                     sensors[i].sensorId, timeSinceLastSeen / 1000);
      }
    }
  }
}

bool isSensorTimedOut(uint8_t sensorId) {
  SensorInfo* sensor = getSensorInfo(sensorId);
  if (sensor == NULL) {
    return false;
  }
  
  uint32_t timeSinceLastSeen = millis() - sensor->lastSeen;
  return (timeSinceLastSeen > 900000);  // 15 minutes
}
