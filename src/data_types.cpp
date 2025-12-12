#include "data_types.h"

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
