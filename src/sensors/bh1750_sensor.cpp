/**
 * @file bh1750_sensor.cpp
 * @brief Implementation of BH1750 light sensor
 * 
 * @version 2.15.0
 * @date 2025-12-14
 */

#include "sensors/bh1750_sensor.h"
#include <Arduino.h>

BH1750Sensor::BH1750Sensor(uint8_t address, const char* sensorName)
    : i2cAddress(address)
    , lightLevel(0.0)
    , lastReadTime(0)
    , readErrorCount(0)
    , connected(false)
{
    strncpy(name, sensorName, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
}

SensorType BH1750Sensor::getType() const {
    return SENSOR_BH1750;
}

InterfaceType BH1750Sensor::getInterface() const {
    return INTERFACE_I2C;
}

const char* BH1750Sensor::getName() const {
    return name;
}

uint8_t BH1750Sensor::getAddress() const {
    return i2cAddress;
}

bool BH1750Sensor::detect() {
    // Try to initialize BH1750
    return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, i2cAddress);
}

bool BH1750Sensor::begin() {
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, i2cAddress)) {
        Serial.printf("BH1750: Failed to initialize at 0x%02X\n", i2cAddress);
        connected = false;
        return false;
    }
    
    connected = true;
    Serial.printf("BH1750: Initialized at 0x%02X\n", i2cAddress);
    
    // Wait for first measurement
    delay(120);
    
    // Perform initial read
    return read();
}

bool BH1750Sensor::read() {
    if (!connected) {
        return false;
    }
    
    float lux = lightMeter.readLightLevel();
    
    if (lux < 0) {
        Serial.println("BH1750: Failed to read light level");
        readErrorCount++;
        return false;
    }
    
    lightLevel = lux;
    lastReadTime = millis();
    return true;
}

bool BH1750Sensor::isConnected() {
    return connected;
}

uint8_t BH1750Sensor::getValueCount() const {
    return 1;  // Light intensity only
}

bool BH1750Sensor::getValue(uint8_t index, SensorValue& value) {
    if (index != 0) {
        return false;
    }
    
    value.type = VALUE_LIGHT;
    value.value = lightLevel;
    value.name = SensorHelpers::getValueName(VALUE_LIGHT);
    value.unit = SensorHelpers::getUnit(VALUE_LIGHT);
    value.deviceClass = SensorHelpers::getDeviceClass(VALUE_LIGHT);
    
    return true;
}

uint32_t BH1750Sensor::getLastReadTime() const {
    return lastReadTime;
}

uint32_t BH1750Sensor::getReadErrorCount() const {
    return readErrorCount;
}

void BH1750Sensor::setMode(uint8_t mode) {
    // Convert uint8_t to BH1750::Mode
    BH1750::Mode modeEnum;
    switch(mode) {
        case 0: modeEnum = BH1750::CONTINUOUS_HIGH_RES_MODE; break;
        case 1: modeEnum = BH1750::CONTINUOUS_HIGH_RES_MODE_2; break;
        case 2: modeEnum = BH1750::CONTINUOUS_LOW_RES_MODE; break;
        case 3: modeEnum = BH1750::ONE_TIME_HIGH_RES_MODE; break;
        case 4: modeEnum = BH1750::ONE_TIME_HIGH_RES_MODE_2; break;
        case 5: modeEnum = BH1750::ONE_TIME_LOW_RES_MODE; break;
        default: modeEnum = BH1750::CONTINUOUS_HIGH_RES_MODE; break;
    }
    lightMeter.begin(modeEnum, i2cAddress);
}
