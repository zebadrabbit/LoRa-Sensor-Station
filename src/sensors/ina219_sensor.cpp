/**
 * @file ina219_sensor.cpp
 * @brief Implementation of INA219 power monitor sensor
 * 
 * @version 2.15.0
 * @date 2025-12-14
 */

#include "sensors/ina219_sensor.h"
#include <Arduino.h>

INA219Sensor::INA219Sensor(uint8_t address, const char* sensorName)
    : i2cAddress(address)
    , voltage(0.0)
    , current(0.0)
    , power(0.0)
    , lastReadTime(0)
    , readErrorCount(0)
    , connected(false)
{
    strncpy(name, sensorName, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
}

SensorType INA219Sensor::getType() const {
    return SENSOR_INA219;
}

InterfaceType INA219Sensor::getInterface() const {
    return INTERFACE_I2C;
}

const char* INA219Sensor::getName() const {
    return name;
}

uint8_t INA219Sensor::getAddress() const {
    return i2cAddress;
}

bool INA219Sensor::detect() {
    // INA219 library doesn't support custom addresses in begin()
    // It uses setAddress() instead - try to detect by beginning with default Wire
    if (!ina219.begin()) {
        return false;
    }
    // Set the I2C address if not default
    if (i2cAddress != 0x40) {
        // Note: Library doesn't expose setAddress(), so we're limited to 0x40
        // For non-default addresses, we'll need to modify the approach
        Serial.printf("INA219: Warning - library only supports 0x40, requested 0x%02X\n", i2cAddress);
    }
    return true;
}

bool INA219Sensor::begin() {
    if (!ina219.begin()) {
        Serial.printf("INA219: Failed to initialize at 0x%02X\n", i2cAddress);
        connected = false;
        return false;
    }
    
    // Set default calibration (32V, 2A range)
    ina219.setCalibration_32V_2A();
    
    connected = true;
    Serial.printf("INA219: Initialized at 0x%02X\n", i2cAddress);
    
    // Perform initial read
    return read();
}

bool INA219Sensor::read() {
    if (!connected) {
        return false;
    }
    
    // Read all values
    float busVoltage = ina219.getBusVoltage_V();
    float shuntVoltage = ina219.getShuntVoltage_mV();
    float current_mA = ina219.getCurrent_mA();
    float power_mW = ina219.getPower_mW();
    
    // Check for valid readings (INA219 returns 0 on error)
    if (busVoltage == 0 && shuntVoltage == 0 && current_mA == 0) {
        Serial.println("INA219: Failed to read sensor");
        readErrorCount++;
        return false;
    }
    
    voltage = busVoltage;
    current = current_mA / 1000.0;  // Convert mA to A
    power = power_mW / 1000.0;      // Convert mW to W
    
    lastReadTime = millis();
    return true;
}

bool INA219Sensor::isConnected() {
    return connected;
}

uint8_t INA219Sensor::getValueCount() const {
    return 3;  // Voltage, Current, Power
}

bool INA219Sensor::getValue(uint8_t index, SensorValue& value) {
    if (index >= 3) {
        return false;
    }
    
    switch (index) {
        case 0:  // Voltage
            value.type = VALUE_VOLTAGE;
            value.value = voltage;
            value.name = SensorHelpers::getValueName(VALUE_VOLTAGE);
            value.unit = SensorHelpers::getUnit(VALUE_VOLTAGE);
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_VOLTAGE);
            break;
            
        case 1:  // Current
            value.type = VALUE_CURRENT;
            value.value = current;
            value.name = SensorHelpers::getValueName(VALUE_CURRENT);
            value.unit = SensorHelpers::getUnit(VALUE_CURRENT);
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_CURRENT);
            break;
            
        case 2:  // Power
            value.type = VALUE_POWER;
            value.value = power;
            value.name = SensorHelpers::getValueName(VALUE_POWER);
            value.unit = SensorHelpers::getUnit(VALUE_POWER);
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_POWER);
            break;
            
        default:
            return false;
    }
    
    return true;
}

uint32_t INA219Sensor::getLastReadTime() const {
    return lastReadTime;
}

uint32_t INA219Sensor::getReadErrorCount() const {
    return readErrorCount;
}

void INA219Sensor::setCalibration32V2A() {
    ina219.setCalibration_32V_2A();
}

void INA219Sensor::setCalibration32V1A() {
    ina219.setCalibration_32V_1A();
}

void INA219Sensor::setCalibration16V400mA() {
    ina219.setCalibration_16V_400mA();
}
