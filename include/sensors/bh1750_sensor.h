/**
 * @file bh1750_sensor.h
 * @brief BH1750 Light Intensity Sensor
 * 
 * @version 2.15.0
 * @date 2025-12-14
 */

#ifndef BH1750_SENSOR_H
#define BH1750_SENSOR_H

#include "sensor_interface.h"
#include <BH1750.h>  // Heltec's built-in BH1750 library

/**
 * @brief BH1750 ambient light sensor
 * 
 * Provides 1 reading:
 * - Light Intensity (lux)
 * 
 * I2C Addresses: 0x23 (default) or 0x5C
 */
class BH1750Sensor : public ISensor {
private:
    BH1750 lightMeter;
    uint8_t i2cAddress;
    char name[32];
    
    float lightLevel;
    
    uint32_t lastReadTime;
    uint32_t readErrorCount;
    bool connected;
    
public:
    BH1750Sensor(uint8_t address = 0x23, const char* sensorName = "BH1750");
    
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
    
    // BH1750 specific configuration
    void setMode(uint8_t mode);  // CONTINUOUS_HIGH_RES_MODE, etc.
};

#endif // BH1750_SENSOR_H
