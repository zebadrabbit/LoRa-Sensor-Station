/**
 * @file sensor_interface.h
 * @brief Abstract interface for modular sensor support
 * 
 * Supports multiple interface types:
 * - I2C: BME680, BH1750, INA219, etc.
 * - 1-Wire: DS18B20 temperature sensors
 * - DHT: DHT22, DHT11 humidity sensors
 * - ADC: Thermistors, photoresistors, analog sensors
 * 
 * @version 2.9.0
 * @date 2025-12-13
 */

#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include <Arduino.h>

/**
 * @brief Sensor type enumeration
 */
enum SensorType {
    SENSOR_THERMISTOR = 0,
    SENSOR_DS18B20 = 1,
    SENSOR_DHT22 = 2,
    SENSOR_DHT11 = 3,
    SENSOR_BME680 = 4,
    SENSOR_BH1750 = 5,
    SENSOR_INA219 = 6,
    SENSOR_SHT31 = 7,
    SENSOR_BMP280 = 8,
    SENSOR_ANALOG_GENERIC = 9,
    SENSOR_PHOTORESISTOR = 10,
    SENSOR_SOIL_MOISTURE = 11
};

/**
 * @brief Sensor interface type
 */
enum InterfaceType {
    INTERFACE_I2C = 0,
    INTERFACE_ONEWIRE = 1,
    INTERFACE_DHT = 2,
    INTERFACE_ADC = 3
};

/**
 * @brief Sensor value type (for Home Assistant device classes)
 */
enum ValueType {
    VALUE_TEMPERATURE = 0,
    VALUE_HUMIDITY = 1,
    VALUE_PRESSURE = 2,
    VALUE_LIGHT = 3,
    VALUE_VOLTAGE = 4,
    VALUE_CURRENT = 5,
    VALUE_POWER = 6,
    VALUE_ENERGY = 7,
    VALUE_GAS_RESISTANCE = 8,
    VALUE_BATTERY = 9,
    VALUE_SIGNAL_STRENGTH = 10,
    VALUE_MOISTURE = 11,
    VALUE_GENERIC = 12
};

/**
 * @brief Sensor value structure
 */
struct SensorValue {
    ValueType type;
    float value;
    const char* name;
    const char* unit;
    const char* deviceClass;  // For Home Assistant
};

/**
 * @brief Abstract sensor interface
 * 
 * All sensor implementations must inherit from this interface
 */
class ISensor {
public:
    virtual ~ISensor() {}
    
    // Identification
    virtual SensorType getType() const = 0;
    virtual InterfaceType getInterface() const = 0;
    virtual const char* getName() const = 0;
    virtual uint8_t getAddress() const = 0;  // I2C address or GPIO pin
    
    // Lifecycle
    virtual bool detect() = 0;      // Auto-detection (returns true if sensor found)
    virtual bool begin() = 0;       // Initialize sensor
    virtual bool read() = 0;        // Read current values
    virtual bool isConnected() = 0; // Check if sensor is responding
    
    // Data access
    virtual uint8_t getValueCount() const = 0;
    virtual bool getValue(uint8_t index, SensorValue& value) = 0;
    
    // Optional features
    virtual bool supportsCalibration() { return false; }
    virtual bool calibrate(float reference) { return false; }
    
    // Status
    virtual uint32_t getLastReadTime() const = 0;
    virtual uint32_t getReadErrorCount() const = 0;
};

/**
 * @brief Helper functions
 */
namespace SensorHelpers {
    // Convert value type to Home Assistant device class
    inline const char* getDeviceClass(ValueType type) {
        switch(type) {
            case VALUE_TEMPERATURE: return "temperature";
            case VALUE_HUMIDITY: return "humidity";
            case VALUE_PRESSURE: return "pressure";
            case VALUE_LIGHT: return "illuminance";
            case VALUE_VOLTAGE: return "voltage";
            case VALUE_CURRENT: return "current";
            case VALUE_POWER: return "power";
            case VALUE_ENERGY: return "energy";
            case VALUE_BATTERY: return "battery";
            case VALUE_SIGNAL_STRENGTH: return "signal_strength";
            case VALUE_MOISTURE: return "moisture";
            default: return "None";
        }
    }
    
    // Convert value type to unit string
    inline const char* getUnit(ValueType type) {
        switch(type) {
            case VALUE_TEMPERATURE: return "°C";
            case VALUE_HUMIDITY: return "%";
            case VALUE_PRESSURE: return "hPa";
            case VALUE_LIGHT: return "lx";
            case VALUE_VOLTAGE: return "V";
            case VALUE_CURRENT: return "A";
            case VALUE_POWER: return "W";
            case VALUE_ENERGY: return "Wh";
            case VALUE_GAS_RESISTANCE: return "Ω";
            case VALUE_BATTERY: return "%";
            case VALUE_SIGNAL_STRENGTH: return "dBm";
            case VALUE_MOISTURE: return "%";
            default: return "";
        }
    }
    
    // Convert value type to readable name
    inline const char* getValueName(ValueType type) {
        switch(type) {
            case VALUE_TEMPERATURE: return "Temperature";
            case VALUE_HUMIDITY: return "Humidity";
            case VALUE_PRESSURE: return "Pressure";
            case VALUE_LIGHT: return "Light";
            case VALUE_VOLTAGE: return "Voltage";
            case VALUE_CURRENT: return "Current";
            case VALUE_POWER: return "Power";
            case VALUE_ENERGY: return "Energy";
            case VALUE_GAS_RESISTANCE: return "Gas Resistance";
            case VALUE_BATTERY: return "Battery";
            case VALUE_SIGNAL_STRENGTH: return "Signal Strength";
            case VALUE_MOISTURE: return "Moisture";
            default: return "Value";
        }
    }
}

#endif // SENSOR_INTERFACE_H
