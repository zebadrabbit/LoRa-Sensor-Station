/**
 * @file thermistor_sensor.cpp
 * @brief Implementation of ThermistorSensor
 * 
 * @version 2.9.0
 * @date 2025-12-13
 */

#include "thermistor_sensor.h"
#include <Arduino.h>
#include <math.h>

ThermistorSensor::ThermistorSensor(uint8_t adcPin, const char* sensorName)
    : pin(adcPin)
    , currentTemperature(0.0)
    , lastReadTime(0)
    , readErrorCount(0)
    , connected(false)
{
    strncpy(name, sensorName, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
}

SensorType ThermistorSensor::getType() const {
    return SENSOR_THERMISTOR;
}

InterfaceType ThermistorSensor::getInterface() const {
    return INTERFACE_ADC;
}

const char* ThermistorSensor::getName() const {
    return name;
}

uint8_t ThermistorSensor::getAddress() const {
    return pin;
}

bool ThermistorSensor::detect() {
    // Try reading ADC
    uint16_t adcValue = analogRead(pin);
    
    // Check if value is in reasonable range (not floating or shorted)
    if (adcValue > 100 && adcValue < 4000) {
        connected = true;
        Serial.printf("ThermistorSensor: Detected on GPIO %d (ADC=%d)\n", pin, adcValue);
        return true;
    }
    
    Serial.printf("ThermistorSensor: Not detected on GPIO %d (ADC=%d out of range)\n", pin, adcValue);
    connected = false;
    return false;
}

bool ThermistorSensor::begin() {
    // Configure ADC pin
    pinMode(pin, INPUT);
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    
    // Initial detection
    return detect();
}

bool ThermistorSensor::read() {
    currentTemperature = readTemperature();
    lastReadTime = millis();
    
    // Validate reading
    if (isnan(currentTemperature) || currentTemperature < -40.0 || currentTemperature > 125.0) {
        readErrorCount++;
        connected = false;
        Serial.printf("ThermistorSensor: Invalid reading (%.2f°C)\n", currentTemperature);
        return false;
    }
    
    connected = true;
    return true;
}

bool ThermistorSensor::isConnected() {
    return connected;
}

uint8_t ThermistorSensor::getValueCount() const {
    return 1;  // Only temperature
}

bool ThermistorSensor::getValue(uint8_t index, SensorValue& value) {
    if (index != 0) {
        return false;
    }
    
    value.type = VALUE_TEMPERATURE;
    value.value = currentTemperature + offsetCalibration;
    value.name = SensorHelpers::getValueName(VALUE_TEMPERATURE);
    value.unit = SensorHelpers::getUnit(VALUE_TEMPERATURE);
    value.deviceClass = SensorHelpers::getDeviceClass(VALUE_TEMPERATURE);
    
    return true;
}

bool ThermistorSensor::supportsCalibration() {
    return true;
}

bool ThermistorSensor::calibrate(float reference) {
    // Set offset to match reference temperature
    offsetCalibration = reference - currentTemperature;
    Serial.printf("ThermistorSensor: Calibrated with offset %.2f°C\n", offsetCalibration);
    return true;
}

uint32_t ThermistorSensor::getLastReadTime() const {
    return lastReadTime;
}

uint32_t ThermistorSensor::getReadErrorCount() const {
    return readErrorCount;
}

void ThermistorSensor::setCoefficients(float a, float b, float c) {
    A = a;
    B = b;
    C = c;
}

void ThermistorSensor::setSeriesResistor(float ohms) {
    seriesResistor = ohms;
}

void ThermistorSensor::setVcc(float voltage) {
    vcc = voltage;
}

float ThermistorSensor::readTemperature() {
    // Read ADC value
    uint16_t adcValue = analogRead(pin);
    
    // Convert to resistance
    float resistance = adcToResistance(adcValue);
    
    // Convert to temperature using Steinhart-Hart
    float temperature = resistanceToTemperature(resistance);
    
    return temperature;
}

float ThermistorSensor::adcToResistance(uint16_t adcValue) {
    // ESP32 ADC: 12-bit (0-4095)
    float voltage = (adcValue / 4095.0) * vcc;
    
    // Voltage divider: V_out = V_cc * (R_thermistor / (R_series + R_thermistor))
    // Solve for R_thermistor:
    float resistance = seriesResistor * voltage / (vcc - voltage);
    
    return resistance;
}

float ThermistorSensor::resistanceToTemperature(float resistance) {
    // Steinhart-Hart equation:
    // 1/T = A + B*ln(R) + C*(ln(R))^3
    // Where T is in Kelvin
    
    float logR = log(resistance);
    float temp_K = 1.0 / (A + B * logR + C * logR * logR * logR);
    float temp_C = temp_K - 273.15;
    
    return temp_C;
}
