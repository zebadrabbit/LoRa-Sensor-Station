/**
 * @file ina219_sensor.h
 * @brief INA219 Power Monitor Sensor (Voltage, Current, Power)
 * 
 * @version 2.15.0
 * @date 2025-12-14
 */

#ifndef INA219_SENSOR_H
#define INA219_SENSOR_H

#include "sensor_interface.h"
#include <Adafruit_INA219.h>

/**
 * @brief INA219 power monitor sensor
 * 
 * Provides 3 readings:
 * - Bus Voltage (V)
 * - Current (A)
 * - Power (W)
 * 
 * I2C Addresses: 0x40 (default), configurable 0x40-0x4F
 */
class INA219Sensor : public ISensor {
private:
    Adafruit_INA219 ina219;
    uint8_t i2cAddress;
    char name[32];
    
    float voltage;
    float current;
    float power;
    
    uint32_t lastReadTime;
    uint32_t readErrorCount;
    bool connected;
    
public:
    INA219Sensor(uint8_t address = 0x40, const char* sensorName = "INA219");
    
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
    
    // INA219 specific configuration
    void setCalibration32V2A();
    void setCalibration32V1A();
    void setCalibration16V400mA();
};

#endif // INA219_SENSOR_H
