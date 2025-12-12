#include "display_control.h"
#include "config.h"
#include "statistics.h"
#include <Wire.h>
#include <WiFi.h>
#include "HT_SSD1306Wire.h"

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static bool displayOn = true;
static uint32_t lastDisplayActivity = 0;
static uint32_t lastPageCycle = 0;
static uint8_t currentPage = 0;

#ifdef BASE_STATION
  #define NUM_PAGES 4  // Status, Sensors, Statistics, Signal Graph
#else
  #define NUM_PAGES 3  // Status, Statistics, Battery
#endif

void initDisplay() {
  // Enable VCC for display (Vext is active LOW)
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  
  // Initialize button
  pinMode(USER_BUTTON, INPUT_PULLUP);
  
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  
  #ifdef BASE_STATION
    display.drawString(0, 0, "Heltec LoRa V3");
    display.drawString(0, 15, "BASE STATION");
    display.drawString(0, 30, "Initializing...");
    display.drawString(0, 45, "Freq: 868 MHz");
  #elif defined(SENSOR_NODE)
    display.drawString(0, 0, "Heltec LoRa V3");
    display.drawString(0, 15, "SENSOR NODE");
    display.drawString(0, 30, String("ID: ") + String(SENSOR_ID));
    display.drawString(0, 45, "Freq: 868 MHz");
  #endif
  
  display.display();
  delay(2000);
  
  lastDisplayActivity = millis();
  lastPageCycle = millis();
}

void updateDisplayTimeout() {
  lastDisplayActivity = millis();
}

bool isDisplayOn() {
  return displayOn;
}

void wakeDisplay() {
  if (!displayOn) {
    displayOn = true;
    digitalWrite(Vext, LOW);
    delay(50);
    display.init();
    display.setFont(ArialMT_Plain_10);
    lastDisplayActivity = millis();
    Serial.println("Display ON");
  } else {
    lastDisplayActivity = millis();
  }
}

void forceNextPage() {
  currentPage = (currentPage + 1) % NUM_PAGES;
  lastPageCycle = millis();
  updateDisplayTimeout();
}

void checkDisplayTimeout() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(USER_BUTTON);
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    delay(50);
    if (digitalRead(USER_BUTTON) == LOW) {
      if (!displayOn) {
        wakeDisplay();
      } else {
        forceNextPage();
      }
    }
  }
  lastButtonState = currentButtonState;
  
  if (displayOn && (millis() - lastDisplayActivity > DISPLAY_TIMEOUT_MS)) {
    displayOn = false;
    display.clear();
    display.display();
    digitalWrite(Vext, HIGH);
    Serial.println("Display OFF (timeout)");
  }
}

void cycleDisplayPages() {
  if (!displayOn) return;
  
  if (millis() - lastPageCycle >= DISPLAY_PAGE_CYCLE_MS) {
    currentPage = (currentPage + 1) % NUM_PAGES;
    lastPageCycle = millis();
  }
  
  #ifdef BASE_STATION
    displayBaseStationPage();
  #elif defined(SENSOR_NODE)
    displaySensorPage();
  #endif
}

// Graphics utilities
void drawSignalGraph(int16_t* rssiHistory, uint8_t size, int x, int y, int width, int height) {
  display.drawRect(x, y, width, height);
  
  int16_t minRssi = -120, maxRssi = -20;
  
  for (uint8_t i = 1; i < size; i++) {
    int16_t rssi1 = rssiHistory[i - 1];
    int16_t rssi2 = rssiHistory[i];
    
    if (rssi1 > -120 && rssi2 > -120) {
      int y1 = y + height - map(rssi1, minRssi, maxRssi, 0, height);
      int y2 = y + height - map(rssi2, minRssi, maxRssi, 0, height);
      int x1 = x + (i - 1) * width / size;
      int x2 = x + i * width / size;
      
      display.drawLine(x1, y1, x2, y2);
    }
  }
}

void drawBatteryIcon(uint8_t percent, int x, int y) {
  display.drawRect(x, y, 20, 10);
  display.fillRect(x + 20, y + 3, 2, 4);
  
  int fillWidth = (percent * 18) / 100;
  if (fillWidth > 0) {
    display.fillRect(x + 1, y + 1, fillWidth, 8);
  }
}

void drawWifiStatus(bool connected, int x, int y) {
  if (connected) {
    display.drawCircle(x + 4, y + 6, 2);
    display.drawCircle(x + 4, y + 6, 4);
    display.drawCircle(x + 4, y + 6, 6);
  } else {
    display.drawLine(x, y, x + 8, y + 8);
    display.drawLine(x + 8, y, x, y + 8);
  }
}

// Base station display pages
void displayBaseStationPage() {
  if (!displayOn) return;
  
  display.clear();
  SystemStats* stats = getStats();
  
  switch (currentPage) {
    case 0: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "BASE STATION");
      
      bool wifiConnected = (strlen(WIFI_SSID) > 0 && WiFi.status() == WL_CONNECTED);
      display.drawString(0, 12, "WiFi: " + String(wifiConnected ? "Connected" : "Off"));
      drawWifiStatus(wifiConnected, 110, 12);
      
      uint8_t sensorCount = getActiveSensorCount();
      display.drawString(0, 24, "Sensors: " + String(sensorCount));
      
      if (stats->lastRxTime > 0) {
        uint32_t secAgo = (millis() - stats->lastRxTime) / 1000;
        display.drawString(0, 36, "Last RX: " + String(secAgo) + "s ago");
      } else {
        display.drawString(0, 36, "Last RX: Never");
      }
      
      display.drawString(110, 54, "1/4");
      break;
    }
    
    case 1: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "ACTIVE SENSORS");
      
      uint8_t sensorCount = getActiveSensorCount();
      if (sensorCount == 0) {
        display.drawString(0, 20, "No sensors");
      } else {
        uint8_t sensorsToShow = (sensorCount > 4) ? 4 : sensorCount;
        for (uint8_t i = 0; i < sensorsToShow; i++) {
          SensorInfo* sensor = getSensorByIndex(i);
          if (sensor) {
            uint32_t secAgo = (millis() - sensor->lastSeen) / 1000;
            String line = "#" + String(sensor->sensorId) + " " + 
                         String(sensor->lastTemperature, 0) + "C " +
                         String(sensor->lastBatteryPercent) + "% " +
                         String(secAgo) + "s";
            display.drawString(0, 12 + i * 12, line);
          }
        }
      }
      
      display.drawString(110, 54, "2/4");
      break;
    }
    
    case 2: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "STATISTICS");
      
      display.drawString(0, 12, "RX Total: " + String(stats->totalRxPackets));
      display.drawString(0, 24, "RX Invalid: " + String(stats->totalRxInvalid));
      
      uint16_t rxSuccess = stats->totalRxPackets > 0 ? 
        (stats->totalRxPackets * 100) / (stats->totalRxPackets + stats->totalRxInvalid) : 0;
      display.drawString(0, 36, "Success: " + String(rxSuccess) + "%");
      
      display.drawString(110, 54, "3/4");
      break;
    }
    
    case 3: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "SIGNAL HISTORY");
      
      drawSignalGraph(stats->rssiHistory, 32, 10, 15, 108, 40);
      
      display.drawString(0, 57, "-120");
      display.drawString(100, 57, "-20dBm");
      display.drawString(110, 54, "4/4");
      break;
    }
  }
  
  display.display();
}

// Sensor display pages
void displaySensorPage() {
  if (!displayOn) return;
  
  display.clear();
  SystemStats* stats = getStats();
  
  switch (currentPage) {
    case 0: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "SENSOR #" + String(SENSOR_ID));
      
      uint32_t uptime = millis() / 1000;
      display.drawString(0, 12, "Uptime: " + String(uptime) + "s");
      
      if (stats->lastTxTime > 0) {
        uint32_t secAgo = (millis() - stats->lastTxTime) / 1000;
        display.drawString(0, 24, "Last TX: " + String(secAgo) + "s ago");
      }
      
      display.drawString(110, 54, "1/3");
      break;
    }
    
    case 1: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "TX STATISTICS");
      
      display.drawString(0, 12, "Attempts: " + String(stats->totalTxAttempts));
      display.drawString(0, 24, "Success: " + String(stats->totalTxSuccess));
      display.drawString(0, 36, "Failed: " + String(stats->totalTxFailed));
      
      uint16_t txSuccess = stats->totalTxAttempts > 0 ?
        (stats->totalTxSuccess * 100) / stats->totalTxAttempts : 0;
      display.drawString(0, 48, "Rate: " + String(txSuccess) + "%");
      
      display.drawString(110, 54, "2/3");
      break;
    }
    
    case 2: {
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "BATTERY STATUS");
      
      display.drawString(0, 20, "Battery: N/A");
      drawBatteryIcon(0, 54, 35);
      
      display.drawString(0, 50, "Connect battery");
      
      display.drawString(110, 54, "3/3");
      break;
    }
  }
  
  display.display();
}
