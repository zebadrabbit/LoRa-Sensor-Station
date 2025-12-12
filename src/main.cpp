#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "config.h"
#include "data_types.h"
#include "led_control.h"
#include "display_control.h"
#include "sensor_readings.h"
#include "lora_comm.h"
#include "statistics.h"

// Global Variables
SensorData sensorData;
uint32_t lastSendTime = 0;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  #ifdef BASE_STATION
    Serial.println("=== Heltec LoRa V3 Base Station ===");
  #elif defined(SENSOR_NODE)
    Serial.println("=== Heltec LoRa V3 Sensor Node ===");
    Serial.printf("Sensor ID: %d\n", SENSOR_ID);
  #else
    Serial.println("ERROR: No mode defined! Set BASE_STATION or SENSOR_NODE");
    while(1) delay(1000);
  #endif
  
  // Initialize Heltec board hardware
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  // Initialize subsystems
  initLED();
  blinkLED(getColorBlue(), 3, 200);
  
  initDisplay();
  initStats();
  initSensors();
  initLoRa();
  
  #ifdef BASE_STATION
    setLED(getColorGreen());
  #elif defined(SENSOR_NODE)
    setLED(getColorPurple());
  #endif
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  Radio.IrqProcess();
  
  // Check display timeout and button press
  checkDisplayTimeout();
  
  // Cycle display pages
  cycleDisplayPages();
  
  #ifdef SENSOR_NODE
    // Sensor mode: Read sensors and transmit periodically
    if (isLoRaIdle() && millis() - lastSendTime >= SENSOR_INTERVAL) {
      lastSendTime = millis();
      
      // Read sensors
      sensorData.syncWord = SYNC_WORD;
      sensorData.sensorId = SENSOR_ID;
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
  #endif
  
  #ifdef BASE_STATION
    // Base station mode: Enter RX mode when idle
    enterRxMode();
  #endif
}
