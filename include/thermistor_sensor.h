/**
 * @file thermistor_sensor.h
 * @brief Thermistor sensor implementation (NTC 10kΩ)
 * 
 * @version 2.9.0
 * @date 2025-12-13
 */

#ifndef THERMISTOR_SENSOR_H
#define THERMISTOR_SENSOR_H

#include "sensor_interface.h"

/**
 * @brief Thermistor sensor using Steinhart-Hart equation
 * 
 * Hardware: 10kΩ NTC thermistor with voltage divider
 */
class ThermistorSensor : public ISensor {
private:
    uint8_t pin;
    float currentTemperature;
    uint32_t lastReadTime;
    uint32_t readErrorCount;
    bool connected;
    char name[32];
    
    // Steinhart-Hart coefficients for 10kΩ NTC
    float A = 0.001129148;
    float B = 0.000234125;
    float C = 0.0000000876741;
    
    // Voltage divider parameters
    float seriesResistor = 10000.0;  // 10kΩ
    float vcc = 3.3;
    
    // Calibration
    float offsetCalibration = 0.0;
    
public:
    ThermistorSensor(uint8_t adcPin, const char* sensorName = "Thermistor");
    
    // ISensor interface
    SensorType getType() const override;
    InterfaceType getInterface() const override;
    const char* getName() const override;
    uint8_t getAddress() const override;
    
    bool detect() override;
    bool begin() override;
    bool read() override;
    bool isConnected() override;
    
    uint8_t getValueCount() const override;
    bool getValue(uint8_t index, SensorValue& value) override;
    
    bool supportsCalibration() override;
    bool calibrate(float reference) override;
    
    uint32_t getLastReadTime() const override;
    uint32_t getReadErrorCount() const override;
    
    // Configuration
    void setCoefficients(float a, float b, float c);
    void setSeriesResistor(float ohms);
    void setVcc(float voltage);
    
private:
    float readTemperature();
    float adcToResistance(uint16_t adcValue);
    float resistanceToTemperature(float resistance);
};

#endif // THERMISTOR_SENSOR_H
