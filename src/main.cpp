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

// Global Variables
SensorData sensorData;
uint32_t lastSendTime = 0;
bool setupComplete = false;

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
    setLED(getColorPurple());
    
  } else if (mode == MODE_BASE_STATION) {
    Serial.println("=== Heltec LoRa V3 Base Station ===");
    BaseStationConfig baseConfig = configStorage.getBaseStationConfig();
    Serial.printf("WiFi SSID: %s\n", baseConfig.ssid);
    
    blinkLED(getColorBlue(), 3, 200);
    
    // Connect to WiFi
    if (wifiPortal.connectToWiFi(baseConfig.ssid, baseConfig.password)) {
      displayMessage("Base Station", "WiFi Connected", WiFi.localIP().toString().c_str(), 2000);
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
    uint32_t interval = sensorConfig.transmitInterval * 1000;
    
    // Check for immediate ping request (triple click)
    bool sendNow = shouldSendImmediatePing();
    if (sendNow) {
      clearImmediatePingFlag();
      Serial.println("Immediate ping requested!");
    }
    
    if (isLoRaIdle() && (sendNow || (millis() - lastSendTime >= interval))) {
      lastSendTime = millis();
      
      // Read sensors
      sensorData.syncWord = SYNC_WORD;
      sensorData.sensorId = sensorConfig.sensorId;
      sensorData.temperature = readThermistor();
      sensorData.batteryVoltage = readBatteryVoltage();
      sensorData.batteryPercent = calculateBatteryPercent(sensorData.batteryVoltage);
      sensorData.powerState = getPowerState();
      sensorData.checksum = calculateChecksum(&sensorData);
      
      // Display readings
      Serial.println("\n--- Sensor Reading ---");
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
    }
  } else if (mode == MODE_BASE_STATION) {
    // Base station mode: Enter RX mode when idle
    enterRxMode();
    
    // Check for sensor timeouts every 30 seconds
    static uint32_t lastTimeoutCheck = 0;
    if (millis() - lastTimeoutCheck >= 30000) {
      lastTimeoutCheck = millis();
      checkSensorTimeouts();
    }
  }
}
