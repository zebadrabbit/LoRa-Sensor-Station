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
    config.configured = (config.sensorId != 0);
    return config;
}

void ConfigStorage::setSensorConfig(const SensorConfig& config) {
    prefs.putUChar("sensor_id", config.sensorId);
    prefs.putString("sensor_loc", config.location);
    prefs.putUShort("tx_interval", config.transmitInterval);
}

BaseStationConfig ConfigStorage::getBaseStationConfig() {
    BaseStationConfig config;
    prefs.getString("wifi_ssid", config.ssid, sizeof(config.ssid));
    prefs.getString("wifi_pass", config.password, sizeof(config.password));
    config.configured = (strlen(config.ssid) > 0);
    return config;
}

void ConfigStorage::setBaseStationConfig(const BaseStationConfig& config) {
    prefs.putString("wifi_ssid", config.ssid);
    prefs.putString("wifi_pass", config.password);
}

void ConfigStorage::clearAll() {
    prefs.clear();
}

bool ConfigStorage::isFirstBoot() {
    return (getDeviceMode() == MODE_UNCONFIGURED);
}
