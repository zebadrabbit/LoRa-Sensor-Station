#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <stdint.h>
#include "sensor_interface.h"

// ====== LEGACY PACKET FORMAT (v1.x - v2.8) ======
// Maintained for backward compatibility
struct SensorData {
  uint16_t syncWord;        // Packet validation
  uint8_t sensorId;         // Client identifier (device with multiple sensors)
  float temperature;        // Temperature in Celsius
  float batteryVoltage;     // Battery voltage
  uint8_t batteryPercent;   // Battery percentage
  bool powerState;          // Power state (charging/discharging)
  uint16_t checksum;        // Simple checksum for validation
};

// ====== NEW VARIABLE-LENGTH PACKET FORMAT (v2.9+) ======

/**
 * @brief Packet types for multi-sensor support
 */
enum PacketType {
    PACKET_LEGACY = 0,      // Old SensorData format (backward compatible)
    PACKET_MULTI_SENSOR = 1, // New variable-length format
    PACKET_CONFIG = 2,      // Configuration packets
    PACKET_ACK = 3          // Acknowledgment packets
};

/**
 * @brief Packet header for multi-sensor data
 */
struct MultiSensorHeader {
    uint16_t syncWord;      // 0xABCD for validation
    uint8_t packetType;     // PacketType enum
    uint8_t sensorId;       // Node ID (which device is sending)
    uint8_t valueCount;     // Number of sensor values in this packet
    uint8_t batteryPercent; // Battery percentage
    uint8_t powerState;     // Power state (charging/discharging)
    uint8_t lastCommandSeq; // Sequence number of last processed command (0 = none)
    uint8_t ackStatus;      // ACK status: 0 = success, non-zero = error code
} __attribute__((packed));

/**
 * @brief Individual sensor value in packet
 */
struct SensorValuePacket {
    uint8_t type;           // ValueType from sensor_interface.h
    float value;            // Actual sensor reading
} __attribute__((packed));

/**
 * @brief Complete multi-sensor packet structure
 * Layout: Header + SensorValuePacket[valueCount] + Checksum
 */
struct MultiSensorPacket {
    MultiSensorHeader header;
    SensorValuePacket values[16];  // Max 16 values per packet
    uint16_t checksum;
} __attribute__((packed));

// Maximum values per packet (LoRa payload limit consideration)
#define MAX_VALUES_PER_PACKET 16
#define MAX_PACKET_SIZE 255

// Utility functions
uint16_t calculateChecksum(SensorData* data);
bool validateChecksum(SensorData* data);

// New utility functions for multi-sensor packets
uint16_t calculateMultiSensorChecksum(MultiSensorPacket* packet);
bool validateMultiSensorChecksum(MultiSensorPacket* packet);
size_t getMultiSensorPacketSize(MultiSensorPacket* packet);

#endif // DATA_TYPES_H
