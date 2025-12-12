#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <stdint.h>

// Data Structure for LoRa packets
struct SensorData {
  uint16_t syncWord;        // Packet validation
  uint8_t sensorId;         // Sensor identifier
  float temperature;        // Temperature in Celsius
  float batteryVoltage;     // Battery voltage
  uint8_t batteryPercent;   // Battery percentage
  bool powerState;          // Power state (charging/discharging)
  uint16_t checksum;        // Simple checksum for validation
};

// Utility functions
uint16_t calculateChecksum(SensorData* data);
bool validateChecksum(SensorData* data);

#endif // DATA_TYPES_H
