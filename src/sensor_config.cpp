#include "sensor_config.h"

SensorConfigManager::SensorConfigManager() {
}

bool SensorConfigManager::begin() {
    return true;
}

String SensorConfigManager::getSensorKey(uint8_t sensorId, const char* field) {
    char key[16];
    snprintf(key, sizeof(key), "s%d_%s", sensorId, field);
    return String(key);
}

SensorMetadata SensorConfigManager::getSensorMetadata(uint8_t sensorId) {
    SensorMetadata metadata;
    metadata.sensorId = sensorId;
    metadata.configured = false;
    
    if (!prefs.begin("sensor-meta", true)) {  // Read-only
        strcpy(metadata.location, "Unknown");
        strcpy(metadata.notes, "");
        metadata.transmitInterval = 15;
        metadata.tempThresholdMin = -40.0;
        metadata.tempThresholdMax = 85.0;
        metadata.alertsEnabled = true;
        return metadata;
    }
    
    // Check if this sensor has custom configuration
    String configKey = getSensorKey(sensorId, "cfg");
    if (!prefs.isKey(configKey.c_str())) {
        prefs.end();
        strcpy(metadata.location, "Unknown");
        strcpy(metadata.notes, "");
        metadata.transmitInterval = 15;
        metadata.tempThresholdMin = -40.0;
        metadata.tempThresholdMax = 85.0;
        metadata.alertsEnabled = true;
        return metadata;
    }
    
    // Load configuration
    metadata.configured = true;
    
    String locKey = getSensorKey(sensorId, "loc");
    String loc = prefs.getString(locKey.c_str(), "Unknown");
    strncpy(metadata.location, loc.c_str(), sizeof(metadata.location) - 1);
    metadata.location[sizeof(metadata.location) - 1] = '\0';
    
    String noteKey = getSensorKey(sensorId, "note");
    String note = prefs.getString(noteKey.c_str(), "");
    strncpy(metadata.notes, note.c_str(), sizeof(metadata.notes) - 1);
    metadata.notes[sizeof(metadata.notes) - 1] = '\0';
    
    String intervalKey = getSensorKey(sensorId, "int");
    metadata.transmitInterval = prefs.getUShort(intervalKey.c_str(), 15);
    
    String minKey = getSensorKey(sensorId, "tmin");
    metadata.tempThresholdMin = prefs.getFloat(minKey.c_str(), -40.0);
    
    String maxKey = getSensorKey(sensorId, "tmax");
    metadata.tempThresholdMax = prefs.getFloat(maxKey.c_str(), 85.0);
    
    String alertKey = getSensorKey(sensorId, "alrt");
    metadata.alertsEnabled = prefs.getBool(alertKey.c_str(), true);
    
    prefs.end();
    return metadata;
}

bool SensorConfigManager::setSensorMetadata(uint8_t sensorId, const SensorMetadata& metadata) {
    if (!prefs.begin("sensor-meta", false)) {  // Read-write
        return false;
    }
    
    // Mark as configured
    String configKey = getSensorKey(sensorId, "cfg");
    prefs.putBool(configKey.c_str(), true);
    
    // Save all fields
    String locKey = getSensorKey(sensorId, "loc");
    prefs.putString(locKey.c_str(), metadata.location);
    
    String noteKey = getSensorKey(sensorId, "note");
    prefs.putString(noteKey.c_str(), metadata.notes);
    
    String intervalKey = getSensorKey(sensorId, "int");
    prefs.putUShort(intervalKey.c_str(), metadata.transmitInterval);
    
    String minKey = getSensorKey(sensorId, "tmin");
    prefs.putFloat(minKey.c_str(), metadata.tempThresholdMin);
    
    String maxKey = getSensorKey(sensorId, "tmax");
    prefs.putFloat(maxKey.c_str(), metadata.tempThresholdMax);
    
    String alertKey = getSensorKey(sensorId, "alrt");
    prefs.putBool(alertKey.c_str(), metadata.alertsEnabled);
    
    prefs.end();
    
    Serial.printf("Sensor %d configuration saved: %s\n", sensorId, metadata.location);
    return true;
}

bool SensorConfigManager::hasSensorMetadata(uint8_t sensorId) {
    if (!prefs.begin("sensor-meta", true)) {
        return false;
    }
    
    String configKey = getSensorKey(sensorId, "cfg");
    bool exists = prefs.isKey(configKey.c_str());
    prefs.end();
    return exists;
}

String SensorConfigManager::getSensorLocation(uint8_t sensorId) {
    SensorMetadata metadata = getSensorMetadata(sensorId);
    return String(metadata.location);
}

void SensorConfigManager::listConfiguredSensors() {
    Serial.println("\n=== Configured Sensors ===");
    
    for (uint8_t id = 1; id <= 255; id++) {
        if (hasSensorMetadata(id)) {
            SensorMetadata metadata = getSensorMetadata(id);
            Serial.printf("Sensor %d: %s", id, metadata.location);
            if (strlen(metadata.notes) > 0) {
                Serial.printf(" (%s)", metadata.notes);
            }
            Serial.printf(" - %ds interval\n", metadata.transmitInterval);
        }
    }
    Serial.println("==========================\n");
}

bool SensorConfigManager::clearSensorMetadata(uint8_t sensorId) {
    if (!prefs.begin("sensor-meta", false)) {
        return false;
    }
    
    // Remove all keys for this sensor
    String configKey = getSensorKey(sensorId, "cfg");
    prefs.remove(configKey.c_str());
    
    String locKey = getSensorKey(sensorId, "loc");
    prefs.remove(locKey.c_str());
    
    String noteKey = getSensorKey(sensorId, "note");
    prefs.remove(noteKey.c_str());
    
    String intervalKey = getSensorKey(sensorId, "int");
    prefs.remove(intervalKey.c_str());
    
    String minKey = getSensorKey(sensorId, "tmin");
    prefs.remove(minKey.c_str());
    
    String maxKey = getSensorKey(sensorId, "tmax");
    prefs.remove(maxKey.c_str());
    
    String alertKey = getSensorKey(sensorId, "alrt");
    prefs.remove(alertKey.c_str());
    
    prefs.end();
    
    Serial.printf("Sensor %d configuration cleared\n", sensorId);
    return true;
}
