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
#ifdef BASE_STATION
#include "mqtt_client.h"
#include "sensor_config.h"
#include "remote_config.h"
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

const char* FIRMWARE_VERSION = "v3.0.0 - Mesh Network Support";

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Heltec board hardware
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  // Initialize LED and display early
  initLED();
  initDisplay();
  
  // Initialize configuration storage
  configStorage.begin();
  
  // Check if this is first boot or if we need configuration
  if (configStorage.isFirstBoot()) {
    Serial.println("=== First Boot - Starting Configuration Portal ===");
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
    Serial.println("=== Heltec LoRa V3 Sensor Node ===");
    SensorConfig sensorConfig = configStorage.getSensorConfig();
    Serial.printf("Sensor ID: %d\n", sensorConfig.sensorId);
    Serial.printf("Location: %s\n", sensorConfig.location);
    Serial.printf("Interval: %d seconds\n", sensorConfig.transmitInterval);
    
    blinkLED(getColorBlue(), 3, 200);
    initStats();
    initSensors();
    initLoRa();
    
    // Initialize mesh router (sensor mode)
    Serial.println("\n=== Initializing Mesh Router ===");
    Serial.printf("Mesh Enabled: %s\n", sensorConfig.meshEnabled ? "YES" : "NO");
    Serial.printf("Mesh Forwarding: %s\n", sensorConfig.meshForwarding ? "YES" : "NO");
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
      Serial.println("Thermistor sensor added successfully");
    } else {
      Serial.println("Failed to initialize thermistor sensor");
      delete thermistor;
    }
    
    // Print sensor status
    sensorManager.printStatus();
    #endif // SENSOR_NODE
    
    setLED(getColorPurple());
    
  } else if (mode == MODE_BASE_STATION) {
    Serial.println("=== Heltec LoRa V3 Base Station ===");
    BaseStationConfig baseConfig = configStorage.getBaseStationConfig();
    Serial.printf("WiFi SSID: %s\n", baseConfig.ssid);
    
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
      Serial.println("Remote configuration manager initialized");
      #endif
      
      // Start web dashboard
      wifiPortal.startDashboard();
      Serial.println("Web dashboard started");
      Serial.println(FIRMWARE_VERSION);
      
    } else {
      // Failed to connect, start portal
      Serial.println("WiFi connection failed - Starting portal");
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
    Serial.println("\n=== Initializing Mesh Router ===");
    Serial.printf("Mesh Enabled: %s\n", baseConfig.meshEnabled ? "YES" : "NO");
    meshRouter.begin(1, true);  // Base station ID = 1
    
    setLED(getColorGreen());
    
  } else {
    Serial.println("ERROR: Invalid device mode!");
    displayMessage("ERROR", "Invalid Mode", "Reset device", 0);
    while(1) delay(1000);
  }
  
  setupComplete = true;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Handle WiFi portal if active
  if (wifiPortal.isPortalActive()) {
    wifiPortal.handleClient();
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
    
    uint32_t interval = sensorConfig.transmitInterval * 1000;
    
    // Check for immediate ping request (triple click)
    bool sendNow = shouldSendImmediatePing();
    if (sendNow) {
      clearImmediatePingFlag();
      Serial.println("Immediate ping requested!");
    }
    
    // Check for immediate ACK send after command processing
    #ifdef SENSOR_NODE
    if (!sendNow && shouldSendImmediateAck()) {
      sendNow = true;
      Serial.println("Immediate ACK send requested!");
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
        sensorData.checksum = calculateChecksum(&sensorData);
        
        // Display readings
        Serial.println("\n--- Sensor Reading (Legacy Format) ---");
        Serial.printf("Temperature: %.2f°C\n", sensorData.temperature);
        Serial.printf("Battery Voltage: %.2fV\n", sensorData.batteryVoltage);
        Serial.printf("Battery Percent: %d%%\n", sensorData.batteryPercent);
        Serial.printf("Power State: %s\n", sensorData.powerState ? "Charging" : "Discharging");
        
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
        Serial.println("\n--- Sensor Reading (Multi-Sensor Format) ---");
        Serial.printf("Sensor ID: %d\n", packet.header.sensorId);
        Serial.printf("Value Count: %d\n", packet.header.valueCount);
        for (int i = 0; i < packet.header.valueCount; i++) {
          Serial.printf("  Value %d: %.2f (type %d)\n", i, packet.values[i].value, packet.values[i].type);
        }
        Serial.printf("Battery Percent: %d%%\n", packet.header.batteryPercent);
        Serial.printf("Power State: %s\n", packet.header.powerState ? "Charging" : "Discharging");
        Serial.printf("Checksum: 0x%04X\n", checksum);
        
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
        
        // Send multi-sensor packet
        Radio.Send(buffer, packetSize);
        Serial.printf("Sending multi-sensor packet (%d bytes)\n", packetSize);
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
    
    // Run mesh router loop only if mesh is enabled
    if (baseConfig.meshEnabled) {
      meshRouter.loop();
    }
    
    // Maintain MQTT connection
    #ifdef BASE_STATION
    mqttClient.loop();
    #endif
    
    // Check for sensor timeouts and alerts every 30 seconds
    static uint32_t lastTimeoutCheck = 0;
    if (millis() - lastTimeoutCheck >= 30000) {
      lastTimeoutCheck = millis();
      checkSensorTimeouts();
      alertManager.checkAllSensors();
    }
  }
}
