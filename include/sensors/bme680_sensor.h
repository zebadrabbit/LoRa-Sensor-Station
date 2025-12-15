/**
 * @file bme680_sensor.h
 * @brief BME680 Environmental Sensor (Temperature, Humidity, Pressure, Gas)
 * 
 * @version 2.15.0
 * @date 2025-12-14
 */

#ifndef BME680_SENSOR_H
#define BME680_SENSOR_H

#include "sensor_interface.h"
#include <Adafruit_BME680.h>

/**
 * @brief BME680 environmental sensor
 * 
 * Provides 4 readings:
 * - Temperature (°C)
 * - Humidity (%)
 * - Pressure (hPa)
 * - Gas Resistance (Ω)
 * 
 * I2C Addresses: 0x76 (default) or 0x77
 */
class BME680Sensor : public ISensor {
private:
    Adafruit_BME680 bme;
    uint8_t i2cAddress;
    char name[32];
    
    float temperature;
    float humidity;
    float pressure;
    float gasResistance;
    
    uint32_t lastReadTime;
    uint32_t readErrorCount;
    bool connected;
    
public:
    BME680Sensor(uint8_t address = 0x76, const char* sensorName = "BME680");
    
    // ISensor interface implementation
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
    
    uint32_t getLastReadTime() const override;
    uint32_t getReadErrorCount() const override;
    
    // BME680 specific configuration
    void setOversampling(uint8_t tempOS, uint8_t humOS, uint8_t pressOS);
    void setIIRFilterSize(uint8_t filterSize);
    void setGasHeater(uint16_t heaterTemp, uint16_t heaterTime);
};

#endif // BME680_SENSOR_H
