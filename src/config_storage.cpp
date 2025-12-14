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
    config.transmitInterval = prefs.getUShort("tx_interval", 30);
    
    // Network ID: if not set, generate random ID on first boot
    config.networkId = prefs.getUShort("network_id", 0);
    if (config.networkId == 0) {
        config.networkId = random(1, 65536);  // Random 1-65535
        prefs.putUShort("network_id", config.networkId);
    }
    
    config.meshEnabled = prefs.getBool("mesh_en", false);  // Disabled by default for backward compatibility
    config.meshForwarding = prefs.getBool("mesh_fwd", true);  // Forwarding enabled by default
    config.configured = (config.sensorId != 0);
    return config;
}

void ConfigStorage::setSensorConfig(const SensorConfig& config) {
    prefs.putUChar("sensor_id", config.sensorId);
    prefs.putString("sensor_loc", config.location);
    prefs.putUShort("tx_interval", config.transmitInterval);
    prefs.putUShort("network_id", config.networkId);
    prefs.putBool("mesh_en", config.meshEnabled);
    prefs.putBool("mesh_fwd", config.meshForwarding);
}

BaseStationConfig ConfigStorage::getBaseStationConfig() {
    BaseStationConfig config;
    prefs.getString("wifi_ssid", config.ssid, sizeof(config.ssid));
    prefs.getString("wifi_pass", config.password, sizeof(config.password));
    
    // Network ID: if not set, generate random ID on first boot
    config.networkId = prefs.getUShort("network_id", 0);
    if (config.networkId == 0) {
        config.networkId = random(1, 65536);  // Random 1-65535
        prefs.putUShort("network_id", config.networkId);
    }
    
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
