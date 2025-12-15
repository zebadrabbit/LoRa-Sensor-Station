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
    
    String zoneKey = getSensorKey(sensorId, "zone");
    String zone = prefs.getString(zoneKey.c_str(), "");
    strncpy(metadata.zone, zone.c_str(), sizeof(metadata.zone) - 1);
    metadata.zone[sizeof(metadata.zone) - 1] = '\0';
    
    String noteKey = getSensorKey(sensorId, "note");
    String note = prefs.getString(noteKey.c_str(), "");
    strncpy(metadata.notes, note.c_str(), sizeof(metadata.notes) - 1);
    metadata.notes[sizeof(metadata.notes) - 1] = '\0';
    
    String intervalKey = getSensorKey(sensorId, "int");
    metadata.transmitInterval = prefs.getUShort(intervalKey.c_str(), 15);
    
    String priorityKey = getSensorKey(sensorId, "prio");
    metadata.priority = (SensorPriority)prefs.getUChar(priorityKey.c_str(), PRIORITY_MEDIUM);
    
    String minKey = getSensorKey(sensorId, "tmin");
    metadata.tempThresholdMin = prefs.getFloat(minKey.c_str(), -40.0);
    
    String maxKey = getSensorKey(sensorId, "tmax");
    metadata.tempThresholdMax = prefs.getFloat(maxKey.c_str(), 85.0);
    
    String alertKey = getSensorKey(sensorId, "alrt");
    metadata.alertsEnabled = prefs.getBool(alertKey.c_str(), true);
    
    // Load health score
    prefs.end();
    metadata.health = getHealthScore(sensorId);
    
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
    
    String zoneKey = getSensorKey(sensorId, "zone");
    prefs.putString(zoneKey.c_str(), metadata.zone);
    
    String noteKey = getSensorKey(sensorId, "note");
    prefs.putString(noteKey.c_str(), metadata.notes);
    
    String intervalKey = getSensorKey(sensorId, "int");
    prefs.putUShort(intervalKey.c_str(), metadata.transmitInterval);
    
    String priorityKey = getSensorKey(sensorId, "prio");
    prefs.putUChar(priorityKey.c_str(), metadata.priority);
    
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

// ============================================================================
// HEALTH SCORING IMPLEMENTATION
// ============================================================================

void SensorConfigManager::updateHealthScore(uint8_t sensorId, bool packetSuccess, float batteryVoltage, float temperature) {
    if (!prefs.begin("sensor-health", false)) {
        return;
    }
    
    // Load current health data
    String totalKey = getSensorKey(sensorId, "htot");
    String failKey = getSensorKey(sensorId, "hfail");
    String uptimeKey = getSensorKey(sensorId, "hupt");
    String lastSeenKey = getSensorKey(sensorId, "hlast");
    String firstSeenKey = getSensorKey(sensorId, "hfirst");
    
    uint16_t totalPackets = prefs.getUShort(totalKey.c_str(), 0);
    uint16_t failedPackets = prefs.getUShort(failKey.c_str(), 0);
    uint32_t firstSeen = prefs.getUInt(firstSeenKey.c_str(), millis());
    
    // Update counters
    totalPackets++;
    if (!packetSuccess) {
        failedPackets++;
    }
    
    uint32_t now = millis();
    uint32_t uptimeSeconds = (now - firstSeen) / 1000;
    
    // Save updated counters
    prefs.putUShort(totalKey.c_str(), totalPackets);
    prefs.putUShort(failKey.c_str(), failedPackets);
    prefs.putUInt(uptimeKey.c_str(), uptimeSeconds);
    prefs.putUInt(lastSeenKey.c_str(), now);
    if (totalPackets == 1) {
        prefs.putUInt(firstSeenKey.c_str(), now);
    }
    
    // Store battery and temperature for quality analysis
    String battKey = getSensorKey(sensorId, "hbatt");
    String tempKey = getSensorKey(sensorId, "htemp");
    prefs.putFloat(battKey.c_str(), batteryVoltage);
    prefs.putFloat(tempKey.c_str(), temperature);
    
    prefs.end();
}

SensorHealthScore SensorConfigManager::getHealthScore(uint8_t sensorId) {
    SensorHealthScore score;
    score.communicationReliability = 0.0;
    score.readingQuality = 1.0;
    score.batteryHealth = 1.0;
    score.overallHealth = 0.0;
    score.uptimeSeconds = 0;
    score.lastSeenTimestamp = 0;
    score.totalPackets = 0;
    score.failedPackets = 0;
    
    if (!prefs.begin("sensor-health", true)) {
        return score;
    }
    
    // Load health data
    String totalKey = getSensorKey(sensorId, "htot");
    String failKey = getSensorKey(sensorId, "hfail");
    String uptimeKey = getSensorKey(sensorId, "hupt");
    String lastSeenKey = getSensorKey(sensorId, "hlast");
    String battKey = getSensorKey(sensorId, "hbatt");
    
    score.totalPackets = prefs.getUShort(totalKey.c_str(), 0);
    score.failedPackets = prefs.getUShort(failKey.c_str(), 0);
    score.uptimeSeconds = prefs.getUInt(uptimeKey.c_str(), 0);
    score.lastSeenTimestamp = prefs.getUInt(lastSeenKey.c_str(), 0);
    float batteryVoltage = prefs.getFloat(battKey.c_str(), 4.2);
    
    prefs.end();
    
    // Calculate component scores
    score.communicationReliability = calculateCommunicationReliability(score.totalPackets, score.failedPackets);
    score.batteryHealth = calculateBatteryHealth(batteryVoltage, score.uptimeSeconds);
    
    // Calculate overall health (weighted average)
    score.overallHealth = (score.communicationReliability * 0.5) +  // 50% weight
                          (score.readingQuality * 0.2) +             // 20% weight  
                          (score.batteryHealth * 0.3);               // 30% weight
    
    return score;
}

float SensorConfigManager::calculateCommunicationReliability(uint16_t totalPackets, uint16_t failedPackets) {
    if (totalPackets == 0) {
        return 0.0;
    }
    
    uint16_t successfulPackets = totalPackets - failedPackets;
    float reliability = (float)successfulPackets / (float)totalPackets;
    
    // Penalize if total packets is very low (not enough data)
    if (totalPackets < 10) {
        reliability *= ((float)totalPackets / 10.0);
    }
    
    return reliability;
}

float SensorConfigManager::calculateReadingQuality(float currentValue, float* history, uint8_t historySize) {
    // Simple variance-based quality
    // Low variance = high quality, high variance = potential issues
    if (historySize < 2) {
        return 1.0;  // Not enough data yet
    }
    
    // Calculate mean
    float sum = 0.0;
    for (uint8_t i = 0; i < historySize; i++) {
        sum += history[i];
    }
    float mean = sum / historySize;
    
    // Calculate variance
    float variance = 0.0;
    for (uint8_t i = 0; i < historySize; i++) {
        float diff = history[i] - mean;
        variance += diff * diff;
    }
    variance /= historySize;
    
    // Convert variance to quality score (0-1)
    // Lower variance = higher quality
    float quality = 1.0 / (1.0 + variance);
    
    return quality;
}

float SensorConfigManager::calculateBatteryHealth(float currentVoltage, uint32_t uptimeSeconds) {
    // Battery health based on voltage and degradation over time
    
    // Voltage-based score (4.2V = 100%, 3.0V = 0%)
    float voltageScore = (currentVoltage - 3.0) / (4.2 - 3.0);
    voltageScore = constrain(voltageScore, 0.0, 1.0);
    
    // Degradation factor (assumes 1% degradation per month of uptime)
    uint32_t uptimeMonths = uptimeSeconds / (30 * 24 * 3600);
    float degradationFactor = 1.0 - (uptimeMonths * 0.01);
    degradationFactor = constrain(degradationFactor, 0.5, 1.0);  // Min 50%
    
    return voltageScore * degradationFactor;
}

// ============================================================================
// ZONE MANAGEMENT
// ============================================================================

String SensorConfigManager::getSensorZone(uint8_t sensorId) {
    SensorMetadata metadata = getSensorMetadata(sensorId);
    return String(metadata.zone);
}

bool SensorConfigManager::setSensorZone(uint8_t sensorId, const char* zone) {
    if (!prefs.begin("sensor-meta", false)) {
        return false;
    }
    
    String zoneKey = getSensorKey(sensorId, "zone");
    prefs.putString(zoneKey.c_str(), zone);
    
    prefs.end();
    
    Serial.printf("Sensor %d zone set to: %s\n", sensorId, zone);
    return true;
}

// ============================================================================
// PRIORITY MANAGEMENT
// ============================================================================

SensorPriority SensorConfigManager::getSensorPriority(uint8_t sensorId) {
    SensorMetadata metadata = getSensorMetadata(sensorId);
    return metadata.priority;
}

bool SensorConfigManager::setSensorPriority(uint8_t sensorId, SensorPriority priority) {
    if (!prefs.begin("sensor-meta", false)) {
        return false;
    }
    
    String priorityKey = getSensorKey(sensorId, "prio");
    prefs.putUChar(priorityKey.c_str(), priority);
    
    prefs.end();
    
    const char* priorityStr = (priority == PRIORITY_HIGH) ? "HIGH" : 
                              (priority == PRIORITY_MEDIUM) ? "MEDIUM" : "LOW";
    Serial.printf("Sensor %d priority set to: %s\n", sensorId, priorityStr);
    return true;
}
