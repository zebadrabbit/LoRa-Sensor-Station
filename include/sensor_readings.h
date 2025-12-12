#ifndef SENSOR_READINGS_H
#define SENSOR_READINGS_H

#include <stdint.h>

// Initialize sensor hardware
void initSensors();

// Sensor reading functions
float readThermistor();
float readBatteryVoltage();
uint8_t calculateBatteryPercent(float voltage);
bool getPowerState();

#endif // SENSOR_READINGS_H
