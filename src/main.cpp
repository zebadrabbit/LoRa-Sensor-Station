#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "config.h"
#include "data_types.h"
#include "led_control.h"
#include "display_control.h"
#include "sensor_readings.h"
#include "lora_comm.h"
#include "statistics.h"
#include "config_storage.h"
#include "wifi_portal.h"
#include "alerts.h"
#include "mesh_routing.h"
#include "security.h"
#include "logger.h"
#ifdef BASE_STATION
#include "mqtt_client.h"
#include "sensor_config.h"
#include "remote_config.h"
#include <time.h>
#include "time_status.h"
#endif
#ifdef SENSOR_NODE
#include "sensor_manager.h"
#include "thermistor_sensor.h"
#include "remote_config.h"
#endif

// Global Variables
SensorData sensorData;
#ifdef BASE_STATION
SensorConfigManager sensorConfigManager;
RemoteConfigManager remoteConfigManager;
#endif
uint32_t lastSendTime = 0;
bool setupComplete = false;
#ifdef SENSOR_NODE
SensorManager sensorManager;
RemoteConfigManager remoteConfigManager;
extern uint8_t lastProcessedCommandSeq;
extern uint8_t lastCommandAckStatus;
#endif

// LoRa settings reboot coordination
bool loraRebootPending = false;
uint32_t loraRebootTime = 0;

const char* FIRMWARE_VERSION = "v3.0.0 - Mesh Network Support";

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  // Initialize unified logger (Serial + LittleFS optional)
  LoggerConfig logCfg;
  logCfg.level = LOG_INFO;
  logCfg.toSerial = true;
  logCfg.toLittleFS = true; // persist logs to LittleFS
  logCfg.toSD = false;      // enable when SD support is available
  logCfg.littlefsPath = "/logs.txt";
  logCfg.sdPath = "/logs.txt";
  loggerBegin(logCfg);
  
  // Initialize Heltec board hardware
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  // Initialize LED and display early
  initLED();
  initDisplay();
  
  // Initialize configuration storage
  configStorage.begin();
  
  // Initialize security module
  LOGI("BOOT", "Initializing Security Module");
  securityManager.begin();
  
  // Check if this is first boot or if we need configuration
  if (configStorage.isFirstBoot()) {
    LOGW("BOOT", "First Boot - Starting Configuration Portal");
    displayMessage("First Boot", "Connect to WiFi AP", "to configure", 2000);
    
    // Start captive portal
    wifiPortal.startPortal();
    
    // Display QR code for easy connection
    displayQRCode("http://10.8.4.1");
    
    while (true) {
      wifiPortal.handleClient();
      delay(10);
    }
  }
  
  // Load configuration
  DeviceMode mode = configStorage.getDeviceMode();
  
  if (mode == MODE_SENSOR) {
    LOGI("SENSOR", "Heltec LoRa V3 Sensor Node");
    SensorConfig sensorConfig = configStorage.getSensorConfig();
    LOGI("SENSOR", "Sensor ID: %d", sensorConfig.sensorId);
    LOGI("SENSOR", "Location: %s", sensorConfig.location);
    LOGI("SENSOR", "Interval: %d seconds", sensorConfig.transmitInterval);
    
    blinkLED(getColorBlue(), 3, 200);
    initStats();
    initSensors();
    initLoRa();
    
    // Initialize mesh router (sensor mode)
    LOGI("MESH", "Initializing Mesh Router");
    LOGI("MESH", "Mesh Enabled: %s", sensorConfig.meshEnabled ? "YES" : "NO");
    LOGI("MESH", "Mesh Forwarding: %s", sensorConfig.meshForwarding ? "YES" : "NO");
    meshRouter.begin(sensorConfig.sensorId, false);  // Not a base station
    meshRouter.setForwardingEnabled(sensorConfig.meshForwarding);
    
    #ifdef SENSOR_NODE
    // Initialize sensor manager
    Serial.println("\n=== Initializing Sensor Manager ===");
    sensorManager.begin();
    
    // Add thermistor sensor on GPIO 1
    ThermistorSensor* thermistor = new ThermistorSensor(1, "Thermistor");
    if (thermistor->begin()) {
      sensorManager.addSensor(thermistor);
      LOGI("SENSOR", "Thermistor sensor added successfully");
    } else {
      LOGE("SENSOR", "Failed to initialize thermistor sensor");
      delete thermistor;
    }
    
    // Print sensor status
    sensorManager.printStatus();
    #endif // SENSOR_NODE
    
    setLED(getColorPurple());
    
  } else if (mode == MODE_BASE_STATION) {
    LOGI("BASE", "Heltec LoRa V3 Base Station");
    BaseStationConfig baseConfig = configStorage.getBaseStationConfig();
    LOGI("BASE", "WiFi SSID: %s", baseConfig.ssid);
    
    blinkLED(getColorBlue(), 3, 200);
    
    // Connect to WiFi
    if (wifiPortal.connectToWiFi(baseConfig.ssid, baseConfig.password)) {
      displayMessage("Base Station", "WiFi Connected", WiFi.localIP().toString().c_str(), 2000);
      
      // Initialize alert manager
      alertManager.begin();
      
      #ifdef BASE_STATION
      // Initialize MQTT client
      mqttClient.begin();
      mqttClient.connect();
      
      // Initialize remote configuration manager
      remoteConfigManager.init();
      LOGI("REMOTE", "Remote configuration manager initialized");
      #endif
      
      // Start web dashboard
      wifiPortal.startDashboard();
      LOGI("WEB", "Web dashboard started");
      LOGI("BOOT", "%s", FIRMWARE_VERSION);
      
      // Initialize NTP if enabled
      NTPConfig ntp = configStorage.getNTPConfig();
      if (ntp.enabled) {
        LOGI("TIME", "NTP Setup - Server: %s, TZ offset: %d min", ntp.server, ntp.tzOffsetMinutes);
        long gmtOffsetSec = (long)ntp.tzOffsetMinutes * 60;
        configTime(gmtOffsetSec, 0, ntp.server);
        #ifdef BASE_STATION
        // Register SNTP sync callback to track last NTP sync time
        registerNtpTimeSyncCallback();
        #endif
      }
      
    } else {
      // Failed to connect, start portal
      LOGW("WEB", "WiFi connection failed - Starting portal");
      displayMessage("WiFi Failed", "Starting AP", "for reconfiguration", 2000);
      wifiPortal.startPortal();
      
      // Display QR code for easy connection
      displayQRCode("http://10.8.4.1");
      
      while (true) {
        wifiPortal.handleClient();
        delay(10);
      }
    }
    
    initStats();
    initLoRa();
    
    // Initialize mesh router (base station mode)
    LOGI("MESH", "Initializing Mesh Router");
    LOGI("MESH", "Mesh Enabled: %s", baseConfig.meshEnabled ? "YES" : "NO");
    meshRouter.begin(1, true);  // Base station ID = 1
    
    // Wait for NTP sync and broadcast time to all sensors on startup
    NTPConfig ntpConfig = configStorage.getNTPConfig();
    if (ntpConfig.enabled) {
      LOGI("TIME", "Waiting for NTP sync before broadcasting to sensors...");
      displayMessage("Time Sync", "Waiting for", "NTP...", 0);
      
      // Wait up to 30 seconds for NTP to sync
      uint32_t ntpWaitStart = millis();
      while (millis() - ntpWaitStart < 30000) {
        time_t now = time(nullptr);
        if (now > 1000000000) {  // Valid time (after year 2001)
          setLastNtpSyncEpoch(now);
          LOGI("TIME", "NTP synced at startup: %lu", (unsigned long)now);
          
          // Broadcast time to all sensors
          extern RemoteConfigManager remoteConfigManager;
          uint8_t payload[6];
          memcpy(&payload[0], &now, sizeof(uint32_t));
          int16_t tz = ntpConfig.tzOffsetMinutes;
          memcpy(&payload[4], &tz, sizeof(int16_t));
          
          int sent = 0;
          for (int i = 1; i < 256; i++) {  // Start from 1 (skip base station)
            if (remoteConfigManager.queueCommand(i, CMD_TIME_SYNC, payload, 6)) {
              sent++;
            }
          }
          LOGI("TIME", "Startup time broadcast queued for %d sensors (epoch=%lu, tz=%d)", sent, (unsigned long)now, (int)tz);
          char msg[32];
          snprintf(msg, sizeof(msg), "%d sensors", sent);
          displayMessage("Time Sync", "Broadcast to", msg, 2000);
          break;
        }
        delay(100);
      }
      
      if (getLastNtpSyncEpoch() == 0) {
        LOGW("TIME", "NTP sync timeout - continuing without time");
        displayMessage("Time Warning", "NTP timeout", "Continuing...", 2000);
      }
    }
    
    setLED(getColorGreen());
    
  } else {
    LOGE("BOOT", "Invalid device mode!");
    displayMessage("ERROR", "Invalid Mode", "Reset device", 0);
    while(1) delay(1000);
  }
  
  setupComplete = true;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Check for pending LoRa settings reboot
  if (loraRebootPending && millis() >= loraRebootTime) {
    Serial.println("\n========================================");
    Serial.println("🔄 REBOOTING TO APPLY NEW LORA SETTINGS");
    Serial.println("========================================\n");
    displayMessage("Rebooting...", "New LoRa", "Settings", 2000);
    delay(2000);
    ESP.restart();
  }
  
  #ifdef BASE_STATION
  // Check for LoRa settings reboot timeout (if not all sensors ACKed within 20s)
  extern void checkLoRaRebootTimeout();
  checkLoRaRebootTimeout();
  #endif
  
  // Handle WiFi portal if active
  if (wifiPortal.isPortalActive()) {
    wifiPortal.handleClient();
  }
  
  // Cleanup WebSocket clients periodically
  static uint32_t lastWsCleanup = 0;
  if (wifiPortal.isDashboardActive() && millis() - lastWsCleanup > 2000) {
    wifiPortal.cleanupWebSocket();
    lastWsCleanup = millis();
  }
  
  if (!setupComplete) {
    return;
  }
  
  Radio.IrqProcess();
  
  #ifdef BASE_STATION
  // Handle any pending WebSocket broadcasts from LoRa ISR
  handlePendingWebSocketBroadcast();
  
  // Check for commands that need retry (every loop)
  checkCommandRetries();
  #endif
  
  // Handle button with multi-click detection
  handleButton();
  
  // Check display timeout
  checkDisplayTimeout();
  
  // Cycle display pages
  cycleDisplayPages();
  
  DeviceMode mode = configStorage.getDeviceMode();
  
  if (mode == MODE_SENSOR) {
    // Sensor mode: Read sensors and transmit periodically
    
    SensorConfig sensorConfig = configStorage.getSensorConfig();
    
    // Run mesh router loop only if mesh is enabled (for beacons and route maintenance)
    if (sensorConfig.meshEnabled) {
      meshRouter.loop();
    }
    
    #ifdef SENSOR_NODE
    // Sensor stays in RX mode continuously
    #endif
    
    // Get configured interval and apply forced interval if command was recently received
    uint32_t configuredInterval = sensorConfig.transmitInterval * 1000;
    #ifdef SENSOR_NODE
    uint32_t interval = getEffectiveTransmitInterval(configuredInterval);
    // Log when forced interval is active
    if (interval != configuredInterval) {
      static uint32_t lastForcedLog = 0;
      if (millis() - lastForcedLog > 30000) {  // Log every 30s
        LOGW("TX", "Using forced 10s interval (configured: %lus)", configuredInterval / 1000);
        lastForcedLog = millis();
      }
    }
    #else
    uint32_t interval = configuredInterval;
    #endif
    
    // Check for immediate ping request (double click)
    bool sendNow = shouldSendImmediatePing();
    if (sendNow) {
      clearImmediatePingFlag();
      LOGI("TX", "Immediate ping requested");
    }
    
    // Check for immediate ACK send after command processing
    #ifdef SENSOR_NODE
    if (!sendNow && shouldSendImmediateAck()) {
      sendNow = true;
      LOGI("TX", "Immediate ACK send requested");
    }
    #endif
    
    if (isLoRaIdle() && (sendNow || (millis() - lastSendTime >= interval))) {
      lastSendTime = millis();
      
      #ifdef SENSOR_NODE
      // Read all sensors
      std::vector<SensorValue> readings = sensorManager.getAllValues();
      
      // Determine packet type based on number of readings
      if (readings.size() == 1 && readings[0].type == VALUE_TEMPERATURE) {
        // Legacy format for backward compatibility
        sensorData.syncWord = SYNC_WORD;
        sensorData.networkId = sensorConfig.networkId;
        sensorData.sensorId = sensorConfig.sensorId;
        sensorData.temperature = readings[0].value;
        sensorData.batteryVoltage = readBatteryVoltage();
        sensorData.batteryPercent = calculateBatteryPercent(sensorData.batteryVoltage);
        sensorData.powerState = getPowerState();
        // Copy location and zone from config
        strncpy(sensorData.location, sensorConfig.location, sizeof(sensorData.location) - 1);
        sensorData.location[sizeof(sensorData.location) - 1] = '\0';
        strncpy(sensorData.zone, sensorConfig.zone, sizeof(sensorData.zone) - 1);
        sensorData.zone[sizeof(sensorData.zone) - 1] = '\0';
        sensorData.checksum = calculateChecksum(&sensorData);
        
        // Display readings
        LOGD("READ", "Legacy Reading: T=%.2fC V=%.2fV B=%d%% P=%s", sensorData.temperature, sensorData.batteryVoltage, sensorData.batteryPercent, sensorData.powerState ? "Charging" : "Discharging");
        
        // Visual feedback based on battery level
        if (sensorData.batteryPercent > 80) {
          setLED(getColorGreen());
        } else if (sensorData.batteryPercent > 50) {
          setLED(getColorYellow());
        } else if (sensorData.batteryPercent > 20) {
          setLED(getColorOrange());
        } else {
          setLED(getColorRed());
        }
        
        sendSensorData(sensorData);
      } else {
        // Multi-sensor format
        MultiSensorPacket packet;
        packet.header.syncWord = 0xABCD;  // Sync word for multi-sensor packets
        packet.header.networkId = sensorConfig.networkId;
        packet.header.packetType = PACKET_MULTI_SENSOR;
        packet.header.sensorId = sensorConfig.sensorId;
        packet.header.valueCount = min((int)readings.size(), MAX_VALUES_PER_PACKET);
        packet.header.batteryPercent = calculateBatteryPercent(readBatteryVoltage());
        packet.header.powerState = getPowerState();
        packet.header.lastCommandSeq = lastProcessedCommandSeq;
        packet.header.ackStatus = lastCommandAckStatus;
        // Copy location and zone from config
        strncpy(packet.header.location, sensorConfig.location, sizeof(packet.header.location) - 1);
        packet.header.location[sizeof(packet.header.location) - 1] = '\0';
        strncpy(packet.header.zone, sensorConfig.zone, sizeof(packet.header.zone) - 1);
        packet.header.zone[sizeof(packet.header.zone) - 1] = '\0';
        
        // Copy sensor values
        for (int i = 0; i < packet.header.valueCount; i++) {
          packet.values[i].type = readings[i].type;
          packet.values[i].value = readings[i].value;
        }
        
        // Calculate checksum and write it to the correct dynamic position
        uint16_t checksum = calculateMultiSensorChecksum(&packet);
        
        // Create buffer with exact packet size
        uint8_t buffer[255];
        size_t headerSize = sizeof(MultiSensorHeader);
        size_t valuesSize = packet.header.valueCount * sizeof(SensorValuePacket);
        size_t packetSize = headerSize + valuesSize + sizeof(uint16_t);
        
        // Copy header
        memcpy(buffer, &packet.header, headerSize);
        
        // Copy values (if any)
        if (packet.header.valueCount > 0) {
          memcpy(buffer + headerSize, packet.values, valuesSize);
        }
        
        // Write checksum at correct position
        memcpy(buffer + headerSize + valuesSize, &checksum, sizeof(uint16_t));
        
        // Display readings
        LOGD("READ", "Multi Reading: sensor=%d values=%d", packet.header.sensorId, packet.header.valueCount);
        for (int i = 0; i < packet.header.valueCount; i++) {
          LOGD("READ", "  Value %d: %.2f (type %d)", i, packet.values[i].value, packet.values[i].type);
        }
        LOGD("READ", "Battery=%d%% Power=%s Checksum=0x%04X", packet.header.batteryPercent, packet.header.powerState ? "Charging" : "Discharging", checksum);
        
        // Visual feedback based on battery level
        if (packet.header.batteryPercent > 80) {
          setLED(getColorGreen());
        } else if (packet.header.batteryPercent > 50) {
          setLED(getColorYellow());
        } else if (packet.header.batteryPercent > 20) {
          setLED(getColorOrange());
        } else {
          setLED(getColorRed());
        }
        
        // Record TX attempt for statistics
        recordTxAttempt();
        
        // Send multi-sensor packet
        Radio.Send(buffer, packetSize);
        LOGI("TX", "Sending multi-sensor packet (%d bytes)", (int)packetSize);
      }
      #else
      // Legacy sensor reading for base station (shouldn't be reached)
      sensorData.syncWord = SYNC_WORD;
      sensorData.networkId = sensorConfig.networkId;
      sensorData.sensorId = sensorConfig.sensorId;
      sensorData.temperature = readThermistor();
      sensorData.batteryVoltage = readBatteryVoltage();
      sensorData.batteryPercent = calculateBatteryPercent(sensorData.batteryVoltage);
      sensorData.powerState = getPowerState();
      sensorData.checksum = calculateChecksum(&sensorData);
      sendSensorData(sensorData);
      #endif // SENSOR_NODE
    }
  } else if (mode == MODE_BASE_STATION) {
    // Base station mode: Enter RX mode when idle
    enterRxMode();
    
    BaseStationConfig baseConfig = configStorage.getBaseStationConfig();
    NTPConfig ntp = configStorage.getNTPConfig();
    
    // Run mesh router loop only if mesh is enabled
    if (baseConfig.meshEnabled) {
      meshRouter.loop();
    }
    
    // Maintain MQTT connection
    #ifdef BASE_STATION
    mqttClient.loop();
    #endif
    
    // Periodic time broadcast to sensors if NTP enabled
    static uint32_t lastTimeBroadcast = 0;
    if (ntp.enabled) {
      uint32_t intervalMs = (ntp.intervalSec < 60 ? 60 : ntp.intervalSec) * 1000UL;
      if (millis() - lastTimeBroadcast >= intervalMs) {
        lastTimeBroadcast = millis();
        #ifdef BASE_STATION
        setLastTimeBroadcastMs(lastTimeBroadcast);
        #endif
        time_t now = time(nullptr);
        if (now > 1700000000) { // sanity check: after 2023-11
          #ifdef BASE_STATION
          setLastNtpSyncEpoch(now); // mark that base has valid NTP-derived time
          #endif
          // Broadcast to all active sensors
          extern RemoteConfigManager remoteConfigManager;
          uint8_t payload[6];
          memcpy(&payload[0], &now, sizeof(uint32_t));
          int16_t tz = ntp.tzOffsetMinutes;
          memcpy(&payload[4], &tz, sizeof(int16_t));
          int sent = 0;
          for (int i = 0; i < 256; i++) {
            SensorInfo* sensor = getSensorInfo(i);
            if (sensor != NULL && !isSensorTimedOut(i)) {
              if (remoteConfigManager.queueCommand(i, CMD_TIME_SYNC, payload, 6)) {
                sent++;
              }
            }
          }
          LOGI("TIME", "Time broadcast sent to %d sensors (epoch=%lu, tz=%d)", sent, (unsigned long)now, (int)tz);
        } else {
          LOGW("TIME", "NTP not synced yet; skipping time broadcast");
        }
      }
    }
    
    // Check for sensor timeouts and alerts every 30 seconds
    static uint32_t lastTimeoutCheck = 0;
    if (millis() - lastTimeoutCheck >= 30000) {
      lastTimeoutCheck = millis();
      checkSensorTimeouts();
      alertManager.checkAllSensors();
    }
  }
}
