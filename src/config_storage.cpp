#include "config_storage.h"

// Global instance
ConfigStorage configStorage;

ConfigStorage::ConfigStorage() {
}

void ConfigStorage::begin() {
    prefs.begin("lora-config", false);
}

DeviceMode ConfigStorage::getDeviceMode() {
    return (DeviceMode)prefs.getUChar("mode", MODE_UNCONFIGURED);
}

void ConfigStorage::setDeviceMode(DeviceMode mode) {
    prefs.putUChar("mode", mode);
}

SensorConfig ConfigStorage::getSensorConfig() {
    SensorConfig config;
    config.sensorId = prefs.getUChar("sensor_id", 0);
    prefs.getString("sensor_loc", config.location, sizeof(config.location));
    // Zone is managed by base station, only read if key exists to avoid error spam
    if (prefs.isKey("sensor_zone")) {
        prefs.getString("sensor_zone", config.zone, sizeof(config.zone));
    } else {
        config.zone[0] = '\0';  // Empty string for sensor nodes
    }
    config.transmitInterval = prefs.getUShort("tx_interval", 30);
    config.networkId = prefs.getUShort("network_id", 12345);  // Default to 12345 if not set
    config.priority = (SensorPriority)prefs.getUChar("priority", PRIORITY_MEDIUM);  // Default to medium
    config.meshEnabled = prefs.getBool("mesh_en", false);  // Disabled by default for backward compatibility
    config.meshForwarding = prefs.getBool("mesh_fwd", true);  // Forwarding enabled by default
    config.configured = (config.sensorId != 0);
    return config;
}

void ConfigStorage::setSensorConfig(const SensorConfig& config) {
    prefs.putUChar("sensor_id", config.sensorId);
    prefs.putString("sensor_loc", config.location);
    prefs.putString("sensor_zone", config.zone);
    prefs.putUShort("tx_interval", config.transmitInterval);
    prefs.putUShort("network_id", config.networkId);
    prefs.putUChar("priority", config.priority);
    prefs.putBool("mesh_en", config.meshEnabled);
    prefs.putBool("mesh_fwd", config.meshForwarding);
}

BaseStationConfig ConfigStorage::getBaseStationConfig() {
    BaseStationConfig config;
    prefs.getString("wifi_ssid", config.ssid, sizeof(config.ssid));
    prefs.getString("wifi_pass", config.password, sizeof(config.password));
    config.networkId = prefs.getUShort("network_id", 12345);  // Default to 12345 if not set
    config.meshEnabled = prefs.getBool("mesh_en", false);  // Disabled by default for backward compatibility
    config.configured = (strlen(config.ssid) > 0);
    return config;
}

void ConfigStorage::setBaseStationConfig(const BaseStationConfig& config) {
    prefs.putString("wifi_ssid", config.ssid);
    prefs.putString("wifi_pass", config.password);
    prefs.putUShort("network_id", config.networkId);
    prefs.putBool("mesh_en", config.meshEnabled);
}

void ConfigStorage::clearAll() {
    prefs.clear();
}

bool ConfigStorage::isFirstBoot() {
    return (getDeviceMode() == MODE_UNCONFIGURED);
}
