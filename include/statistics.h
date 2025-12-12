#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>
#include "data_types.h"

// Sensor information tracking
struct SensorInfo {
  uint8_t sensorId;
  uint32_t lastSeen;
  int16_t lastRssi;
  int8_t lastSnr;
  uint32_t packetsReceived;
  float lastTemperature;
  uint8_t lastBatteryPercent;
  bool active;
};

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

// Initialize statistics
void initStats();

// Update statistics
void recordTxAttempt();
void recordTxSuccess();
void recordTxFailure();
void recordRxPacket(int16_t rssi);
void recordRxInvalid();

// Sensor tracking (base station only)
void updateSensorInfo(const SensorData& data, int16_t rssi, int8_t snr);
uint8_t getActiveSensorCount();
SensorInfo* getSensorInfo(uint8_t sensorId);
SensorInfo* getSensorByIndex(uint8_t index);
void checkSensorTimeouts();
bool isSensorTimedOut(uint8_t sensorId);

// Get statistics
SystemStats* getStats();

#endif // STATISTICS_H
