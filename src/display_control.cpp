#include "display_control.h"
#include "config.h"
#include "statistics.h"
#include "config_storage.h"
#include <Wire.h>
#include <WiFi.h>
#include "HT_SSD1306Wire.h"
#include <qrcode.h>

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static bool displayOn = true;
static uint32_t lastDisplayActivity = 0;
static uint32_t lastPageCycle = 0;
static uint8_t currentPage = 0;

// Button state tracking
static bool lastButtonState = HIGH;
static unsigned long buttonPressTime = 0;
static unsigned long buttonReleaseTime = 0;
static uint8_t clickCount = 0;
static bool buttonHeld = false;
static bool factoryResetTriggered = false;
static bool immediatePingRequested = false;

#ifdef BASE_STATION
  #define NUM_PAGES 5  // Status, Sensors, Statistics, Signal Graph, Battery
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

void displayMessage(const char* line1, const char* line2, const char* line3, uint16_t duration) {
  if (!displayOn) {
    wakeDisplay();
  }
  
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, line1);
  display.drawString(0, 25, line2);
  display.drawString(0, 40, line3);
  display.display();
  
  if (duration > 0) {
    delay(duration);
  }
}

void displayQRCode(const char* url) {
  if (!displayOn) {
    wakeDisplay();
  }

  // Clear display buffer first in normal orientation
  display.clear();
  
  // Rotate display 90 degrees for QR code (portrait mode)
  display.screenRotate(ANGLE_270_DEGREE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  // Clear again after rotation and draw black background
  display.clear();
  display.setColor(BLACK);
  display.fillRect(0, 0, 64, 128);
  display.setColor(WHITE);

  // Create QR code
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, url);
  
  // Draw QR code in portrait mode (64x128 after rotation)
  // QR code version 3 is 29x29 modules
  int scale = 2;  // 2 pixels per module = 58x58 pixels
  int offsetX = (64 - (qrcode.size * scale)) / 2;  // Center in 64px width
  int offsetY = 5;  // Top margin
  
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.setPixel(offsetX + (x * scale), offsetY + (y * scale));
        display.setPixel(offsetX + (x * scale) + 1, offsetY + (y * scale));
        display.setPixel(offsetX + (x * scale), offsetY + (y * scale) + 1);
        display.setPixel(offsetX + (x * scale) + 1, offsetY + (y * scale) + 1);
      }
    }
  }
  
  // Display text below QR code (QR is 58px tall + 5px offset = ends at 63, text at 70)
  display.setFont(ArialMT_Plain_10);
  display.drawString(5, 70, "AP Pass:");
  display.drawString(5, 82, "configure"); 
  
  display.display();
}

void handleButton() {
  bool currentButtonState = digitalRead(USER_BUTTON);
  unsigned long now = millis();
  
  // Detect button press (falling edge)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    delay(50);  // Debounce
    if (digitalRead(USER_BUTTON) == LOW) {
      buttonPressTime = now;
      buttonHeld = false;
      factoryResetTriggered = false;
    }
  }
  
  // While button is held down
  if (currentButtonState == LOW && buttonPressTime > 0) {
    unsigned long holdDuration = now - buttonPressTime;
    
    // 5-second hold = Factory Reset
    if (holdDuration >= 5000 && !factoryResetTriggered && displayOn) {
      factoryResetTriggered = true;
      Serial.println("Factory reset triggered!");
      displayMessage("Factory Reset", "Clearing config...", "Rebooting...", 0);
      configStorage.clearAll();
      delay(2000);
      ESP.restart();
    }
    
    buttonHeld = (holdDuration > 500);  // Mark as held if > 500ms
  }
  
  // Detect button release (rising edge)
  if (lastButtonState == LOW && currentButtonState == HIGH) {
    delay(50);  // Debounce
    if (digitalRead(USER_BUTTON) == HIGH) {
      buttonReleaseTime = now;
      
      // Only count as click if not held and not factory reset
      if (!buttonHeld && !factoryResetTriggered) {
        clickCount++;
      }
      
      buttonPressTime = 0;
    }
  }
  
  // Process clicks after 400ms timeout
  if (clickCount > 0 && (now - buttonReleaseTime > 400)) {
    if (clickCount == 1) {
      // Single click
      if (!displayOn) {
        Serial.println("Single click: Wake display");
        wakeDisplay();
      } else {
        Serial.println("Single click: Next page");
        forceNextPage();
      }
    } else if (clickCount == 2 && displayOn) {
      // Double click = Reboot
      Serial.println("Double click: Rebooting...");
      displayMessage("Rebooting...", "", "", 1000);
      ESP.restart();
    } else if (clickCount == 3 && displayOn) {
      // Triple click = Send immediate ping
      Serial.println("Triple click: Sending immediate ping");
      immediatePingRequested = true;
      displayMessage("Sending", "Ping...", "", 800);
    }
    
    clickCount = 0;
  }
  
  lastButtonState = currentButtonState;
}

void checkDisplayTimeout() {
  // Handle display timeout
  if (displayOn && (millis() - lastDisplayActivity > DISPLAY_TIMEOUT_MS)) {
    displayOn = false;
    display.clear();
    display.display();
    digitalWrite(Vext, HIGH);
    Serial.println("Display OFF (timeout)");
  }
}

bool shouldSendImmediatePing() {
  return immediatePingRequested;
}

void clearImmediatePingFlag() {
  immediatePingRequested = false;
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
      int y1 = y + height - map(rssi1, minRssi, maxRssi, 1, height - 1);
      int y2 = y + height - map(rssi2, minRssi, maxRssi, 1, height - 1);
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
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "BASE STATION");
      display.setColor(WHITE);
      
      bool wifiConnected = (WiFi.status() == WL_CONNECTED);
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
      
      display.drawString(110, 54, "1/5");
      break;
    }
    
    case 1: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "ACTIVE SENSORS");
      display.setColor(WHITE);
      
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
      
      display.drawString(110, 54, "2/5");
      break;
    }
    
    case 2: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "STATISTICS");
      display.setColor(WHITE);
      
      display.drawString(0, 12, "RX Total: " + String(stats->totalRxPackets));
      display.drawString(0, 24, "RX Invalid: " + String(stats->totalRxInvalid));
      
      uint16_t rxSuccess = stats->totalRxPackets > 0 ? 
        (stats->totalRxPackets * 100) / (stats->totalRxPackets + stats->totalRxInvalid) : 0;
      display.drawString(0, 36, "Success: " + String(rxSuccess) + "%");
      
      display.drawString(110, 54, "3/5");
      break;
    }
    
    case 3: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "SIGNAL HISTORY");
      display.setColor(WHITE);
      
      drawSignalGraph(stats->rssiHistory, 32, 10, 13, 108, 36);
      
      display.drawString(0, 52, "-120");
      display.drawString(90, 52, "-20");
      display.drawString(110, 52, "4/5");
      break;
    }
    
    case 4: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "BATTERY STATUS");
      display.setColor(WHITE);
      
      #ifdef BASE_STATION
        float batteryVoltage = readBatteryVoltage();
        uint8_t batteryPercent = calculateBatteryPercent(batteryVoltage);
        bool powerState = getPowerState();
        
        display.drawString(0, 14, "Voltage: " + String(batteryVoltage, 2) + "V");
        display.drawString(0, 26, "Level: " + String(batteryPercent) + "%");
        display.drawString(0, 38, String(powerState ? "Charging" : "Discharging"));
        drawBatteryIcon(batteryPercent, 90, 26);
      #endif
      
      display.drawString(110, 54, "5/5");
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
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "SENSOR #" + String(SENSOR_ID));
      display.setColor(WHITE);
      
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
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "TX STATISTICS");
      display.setColor(WHITE);
      
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
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "BATTERY STATUS");
      display.setColor(WHITE);
      
      display.drawString(0, 20, "Battery: N/A");
      drawBatteryIcon(0, 54, 35);
      
      display.drawString(0, 50, "Connect battery");
      
      display.drawString(110, 54, "3/3");
      break;
    }
  }
  
  display.display();
}
