/**
 * @file bme680_sensor.cpp
 * @brief Implementation of BME680 environmental sensor
 * 
 * @version 2.15.0
 * @date 2025-12-14
 */

#include "sensors/bme680_sensor.h"
#include <Arduino.h>

BME680Sensor::BME680Sensor(uint8_t address, const char* sensorName)
    : i2cAddress(address)
    , temperature(0.0)
    , humidity(0.0)
    , pressure(0.0)
    , gasResistance(0.0)
    , lastReadTime(0)
    , readErrorCount(0)
    , connected(false)
{
    strncpy(name, sensorName, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
}

SensorType BME680Sensor::getType() const {
    return SENSOR_BME680;
}

InterfaceType BME680Sensor::getInterface() const {
    return INTERFACE_I2C;
}

const char* BME680Sensor::getName() const {
    return name;
}

uint8_t BME680Sensor::getAddress() const {
    return i2cAddress;
}

bool BME680Sensor::detect() {
    // Try to initialize BME680
    return bme.begin(i2cAddress);
}

bool BME680Sensor::begin() {
    if (!bme.begin(i2cAddress)) {
        Serial.printf("BME680: Failed to initialize at 0x%02X\n", i2cAddress);
        connected = false;
        return false;
    }
    
    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C for 150 ms
    
    connected = true;
    Serial.printf("BME680: Initialized at 0x%02X\n", i2cAddress);
    
    // Perform initial read
    return read();
}

bool BME680Sensor::read() {
    if (!connected) {
        return false;
    }
    
    if (!bme.performReading()) {
        Serial.println("BME680: Failed to perform reading");
        readErrorCount++;
        return false;
    }
    
    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0;  // Convert Pa to hPa
    gasResistance = bme.gas_resistance / 1000.0;  // Convert Ω to kΩ
    
    lastReadTime = millis();
    return true;
}

bool BME680Sensor::isConnected() {
    return connected;
}

uint8_t BME680Sensor::getValueCount() const {
    return 4;  // Temperature, Humidity, Pressure, Gas Resistance
}

bool BME680Sensor::getValue(uint8_t index, SensorValue& value) {
    if (index >= 4) {
        return false;
    }
    
    switch (index) {
        case 0:  // Temperature
            value.type = VALUE_TEMPERATURE;
            value.value = temperature;
            value.name = SensorHelpers::getValueName(VALUE_TEMPERATURE);
            value.unit = SensorHelpers::getUnit(VALUE_TEMPERATURE);
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_TEMPERATURE);
            break;
            
        case 1:  // Humidity
            value.type = VALUE_HUMIDITY;
            value.value = humidity;
            value.name = SensorHelpers::getValueName(VALUE_HUMIDITY);
            value.unit = SensorHelpers::getUnit(VALUE_HUMIDITY);
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_HUMIDITY);
            break;
            
        case 2:  // Pressure
            value.type = VALUE_PRESSURE;
            value.value = pressure;
            value.name = SensorHelpers::getValueName(VALUE_PRESSURE);
            value.unit = SensorHelpers::getUnit(VALUE_PRESSURE);
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_PRESSURE);
            break;
            
        case 3:  // Gas Resistance
            value.type = VALUE_GAS_RESISTANCE;
            value.value = gasResistance;
            value.name = SensorHelpers::getValueName(VALUE_GAS_RESISTANCE);
            value.unit = "kΩ";  // Custom unit
            value.deviceClass = SensorHelpers::getDeviceClass(VALUE_GAS_RESISTANCE);
            break;
            
        default:
            return false;
    }
    
    return true;
}

uint32_t BME680Sensor::getLastReadTime() const {
    return lastReadTime;
}

uint32_t BME680Sensor::getReadErrorCount() const {
    return readErrorCount;
}

void BME680Sensor::setOversampling(uint8_t tempOS, uint8_t humOS, uint8_t pressOS) {
    bme.setTemperatureOversampling(tempOS);
    bme.setHumidityOversampling(humOS);
    bme.setPressureOversampling(pressOS);
}

void BME680Sensor::setIIRFilterSize(uint8_t filterSize) {
    bme.setIIRFilterSize(filterSize);
}

void BME680Sensor::setGasHeater(uint16_t heaterTemp, uint16_t heaterTime) {
    bme.setGasHeater(heaterTemp, heaterTime);
}
