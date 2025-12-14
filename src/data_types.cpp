#include "data_types.h"

// ====== LEGACY CHECKSUM FUNCTIONS ======

uint16_t calculateChecksum(SensorData* data) {
  uint16_t sum = 0;
  sum += data->syncWord;
  sum += data->sensorId;
  sum += (uint16_t)(data->temperature * 100);
  sum += (uint16_t)(data->batteryVoltage * 100);
  sum += data->batteryPercent;
  sum += data->powerState ? 1 : 0;
  return sum;
}

bool validateChecksum(SensorData* data) {
  uint16_t expected = calculateChecksum(data);
  return expected == data->checksum;
}

// ====== NEW MULTI-SENSOR CHECKSUM FUNCTIONS ======

uint16_t calculateMultiSensorChecksum(MultiSensorPacket* packet) {
    uint16_t sum = 0;
    
    // Add header fields
    sum += packet->header.syncWord;
    sum += packet->header.packetType;
    sum += packet->header.sensorId;
    sum += packet->header.valueCount;
    sum += packet->header.batteryPercent;
    sum += packet->header.powerState;
    sum += packet->header.lastCommandSeq;
    sum += packet->header.ackStatus;
    
    // Add all sensor values
    for (uint8_t i = 0; i < packet->header.valueCount && i < MAX_VALUES_PER_PACKET; i++) {
        sum += packet->values[i].type;
        sum += (uint16_t)(packet->values[i].value * 100);
    }
    
    return sum;
}

bool validateMultiSensorChecksum(MultiSensorPacket* packet) {
    uint16_t expected = calculateMultiSensorChecksum(packet);
    return expected == packet->checksum;
}

size_t getMultiSensorPacketSize(MultiSensorPacket* packet) {
    // Header + (valueCount * SensorValuePacket) + checksum
    return sizeof(MultiSensorHeader) + 
           (packet->header.valueCount * sizeof(SensorValuePacket)) + 
           sizeof(uint16_t);
}

