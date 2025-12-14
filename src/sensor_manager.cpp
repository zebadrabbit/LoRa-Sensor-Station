/**
 * @file sensor_manager.cpp
 * @brief Implementation of SensorManager
 * 
 * @version 2.9.0
 * @date 2025-12-13
 */

#include "sensor_manager.h"
#include <Preferences.h>

SensorManager::SensorManager() 
    : i2cInitialized(false)
    , lastScanTime(0)
    , scanInterval(60000)  // Default: scan every minute
{
}

SensorManager::~SensorManager() {
    clearAll();
}

void SensorManager::begin() {
    Serial.println("SensorManager: Initializing...");
    
    // I2C will be initialized separately (shared with OLED)
    // Load configuration from NVS
    loadConfig();
    
    Serial.printf("SensorManager: Ready with %d sensors\n", sensors.size());
}

void SensorManager::initI2C(uint8_t sda, uint8_t scl) {
    if (i2cInitialized) {
        return;
    }
    
    Wire.begin(sda, scl);
    Wire.setClock(100000);  // 100kHz for compatibility
    i2cInitialized = true;
    
    Serial.printf("SensorManager: I2C initialized (SDA=%d, SCL=%d)\n", sda, scl);
}

void SensorManager::scanI2C() {
    if (!i2cInitialized) {
        Serial.println("SensorManager: I2C not initialized, skipping scan");
        return;
    }
    
    Serial.println("SensorManager: Scanning I2C bus...");
    uint8_t devicesFound = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("  Found device at 0x%02X\n", address);
            devicesFound++;
            
            // Skip if already in use (e.g., OLED display at 0x3C)
            if (isI2CAddressInUse(address)) {
                Serial.printf("    Address 0x%02X already in use, skipping\n", address);
                continue;
            }
            
            // Try to create appropriate sensor
            ISensor* sensor = createSensorFromI2C(address);
            if (sensor) {
                sensors.push_back(sensor);
                Serial.printf("    Created sensor: %s\n", sensor->getName());
            }
        }
    }
    
    Serial.printf("SensorManager: Scan complete, found %d devices\n", devicesFound);
}

void SensorManager::scanOneWire(uint8_t pin) {
    // TODO: Implement DS18B20 scanning in Phase 3
    Serial.printf("SensorManager: OneWire scanning on GPIO %d not yet implemented\n", pin);
}

void SensorManager::addSensor(ISensor* sensor) {
    if (sensor) {
        sensors.push_back(sensor);
        Serial.printf("SensorManager: Added sensor: %s\n", sensor->getName());
    }
}

void SensorManager::removeSensor(uint8_t index) {
    if (index < sensors.size()) {
        ISensor* sensor = sensors[index];
        Serial.printf("SensorManager: Removing sensor: %s\n", sensor->getName());
        
        sensors.erase(sensors.begin() + index);
        delete sensor;
    }
}

void SensorManager::clearAll() {
    for (ISensor* sensor : sensors) {
        delete sensor;
    }
    sensors.clear();
    Serial.println("SensorManager: All sensors removed");
}

bool SensorManager::readAll() {
    bool allSuccess = true;
    
    for (ISensor* sensor : sensors) {
        if (!sensor->read()) {
            Serial.printf("SensorManager: Failed to read %s\n", sensor->getName());
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

std::vector<SensorValue> SensorManager::getAllValues() {
    std::vector<SensorValue> allValues;
    
    // Read all sensors first
    readAll();
    
    // Collect all values from all sensors
    for (ISensor* sensor : sensors) {
        if (sensor->isConnected()) {
            for (uint8_t i = 0; i < sensor->getValueCount(); i++) {
                SensorValue value;
                if (sensor->getValue(i, value)) {
                    allValues.push_back(value);
                }
            }
        }
    }
    
    return allValues;
}

void SensorManager::printStatus() {
    Serial.printf("\n=== Sensor Status (%d sensors) ===\n", sensors.size());
    
    for (size_t i = 0; i < sensors.size(); i++) {
        ISensor* sensor = sensors[i];
        Serial.printf("[%d] %s (0x%02X) - ", i, sensor->getName(), sensor->getAddress());
        
        if (!sensor->isConnected()) {
            Serial.println("DISCONNECTED");
            continue;
        }
        
        uint8_t valueCount = sensor->getValueCount();
        Serial.printf("%d values:\n", valueCount);
        
        for (uint8_t v = 0; v < valueCount; v++) {
            SensorValue value;
            if (sensor->getValue(v, value)) {
                Serial.printf("    %s: %.2f %s\n", value.name, value.value, value.unit);
            }
        }
    }
    
    Serial.println("==============================\n");
}

ISensor* SensorManager::getSensor(uint8_t index) {
    if (index < sensors.size()) {
        return sensors[index];
    }
    return nullptr;
}

uint8_t SensorManager::getSensorCount() const {
    return sensors.size();
}

bool SensorManager::loadConfig() {
    // TODO: Load sensor configurations from NVS
    // For now, return true (will implement in Phase 6)
    return true;
}

bool SensorManager::saveConfig() {
    // TODO: Save sensor configurations to NVS
    // For now, return true (will implement in Phase 6)
    return true;
}

bool SensorManager::enableSensor(uint8_t index, bool enabled) {
    if (index < sensors.size()) {
        // TODO: Mark sensor as enabled/disabled in config
        return true;
    }
    return false;
}

bool SensorManager::renameSensor(uint8_t index, const char* name) {
    if (index < sensors.size() && name) {
        // TODO: Update sensor name in config
        return true;
    }
    return false;
}

void SensorManager::setAutoScanInterval(uint32_t ms) {
    scanInterval = ms;
}

void SensorManager::autoScan() {
    uint32_t now = millis();
    if (now - lastScanTime >= scanInterval) {
        lastScanTime = now;
        scanI2C();
    }
}

String SensorManager::toJSON() {
    String json = "[";
    
    for (size_t i = 0; i < sensors.size(); i++) {
        if (i > 0) json += ",";
        
        ISensor* sensor = sensors[i];
        json += "{";
        json += "\"name\":\"" + String(sensor->getName()) + "\",";
        json += "\"type\":" + String(sensor->getType()) + ",";
        json += "\"address\":" + String(sensor->getAddress()) + ",";
        json += "\"connected\":" + String(sensor->isConnected() ? "true" : "false") + ",";
        json += "\"values\":[";
        
        uint8_t valueCount = sensor->getValueCount();
        for (uint8_t v = 0; v < valueCount; v++) {
            if (v > 0) json += ",";
            
            SensorValue value;
            if (sensor->getValue(v, value)) {
                json += "{";
                json += "\"name\":\"" + String(value.name) + "\",";
                json += "\"value\":" + String(value.value, 2) + ",";
                json += "\"unit\":\"" + String(value.unit) + "\"";
                json += "}";
            }
        }
        
        json += "]}";
    }
    
    json += "]";
    return json;
}

bool SensorManager::getSensorValue(uint8_t sensorIndex, uint8_t valueIndex, SensorValue& value) {
    if (sensorIndex < sensors.size()) {
        return sensors[sensorIndex]->getValue(valueIndex, value);
    }
    return false;
}

ISensor* SensorManager::createSensorFromI2C(uint8_t address) {
    // Phase 2: Will implement I2C sensor detection
    // For now, return nullptr
    Serial.printf("SensorManager: I2C sensor creation at 0x%02X not yet implemented\n", address);
    return nullptr;
}

bool SensorManager::isI2CAddressInUse(uint8_t address) {
    // Check if address is used by existing systems
    if (address == 0x3C || address == 0x3D) {
        return true;  // OLED display
    }
    
    // Check if already in sensor list
    for (ISensor* sensor : sensors) {
        if (sensor->getInterface() == INTERFACE_I2C && sensor->getAddress() == address) {
            return true;
        }
    }
    
    return false;
}
