#include "display_control.h"
#include "config.h"
#include "statistics.h"
#include "config_storage.h"
#include "security.h"
#include "time_status.h"
#include <Wire.h>
#include <WiFi.h>
#include "HT_SSD1306Wire.h"
#include <qrcode.h>
#include <Preferences.h>
#include <time.h>

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

// Command notification state
static bool showingCommandNotif = false;
static uint32_t commandNotifStartTime = 0;
static const uint32_t COMMAND_NOTIF_DURATION = 2000; // 2 seconds

#ifdef BASE_STATION
  #define NUM_PAGES 8  // + Time & NTP
#else
  #define NUM_PAGES 6  // + Time & Sync
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
  
  // Read frequency from NVS (or use default)
  Preferences prefs;
  prefs.begin("lora_params", true);  // Read-only
  uint32_t frequency = prefs.getUInt("frequency", RF_FREQUENCY);
  prefs.end();
  float freqMHz = frequency / 1000000.0;
  
  #ifdef BASE_STATION
    display.drawString(0, 0, "Heltec LoRa V3");
    display.drawString(0, 15, "BASE STATION");
    display.drawString(0, 30, "Initializing...");
    display.drawString(0, 45, "Freq: " + String(freqMHz, 1) + " MHz");
  #elif defined(SENSOR_NODE)
    // Read sensor ID from config
    SensorConfig sensorConfig = configStorage.getSensorConfig();
    display.drawString(0, 0, "Heltec LoRa V3");
    display.drawString(0, 15, "SENSOR NODE");
    display.drawString(0, 30, String("ID: ") + String(sensorConfig.sensorId));
    display.drawString(0, 45, "Freq: " + String(freqMHz, 1) + " MHz");
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
      // Double click = Send immediate ping
      Serial.println("Double click: Sending immediate ping");
      immediatePingRequested = true;
      displayMessage("Sending", "Ping...", "", 800);
    } else if (clickCount == 3 && displayOn) {
      // Triple click = Reboot
      Serial.println("Triple click: Rebooting...");
      displayMessage("Rebooting...", "", "", 1000);
      ESP.restart();
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
#ifdef BASE_STATION
void displayBaseStationPage() {
  if (!displayOn) return;
  
  display.clear();
  SystemStats* stats = getStats();
  
  switch (currentPage) {
    case 0: {
      // Welcome page
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      
      display.drawString(64, 15, "Hello! I am");
      
      String name = "Base Station!";
      // Use smaller font for long names to fit more characters
      if (name.length() > 14) {
        display.setFont(ArialMT_Plain_10);
        display.drawString(64, 38, name);
      } else {
        display.drawString(64, 35, name);
      }
      
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_10);
      display.drawString(110, 54, "1/8");
      break;
    }
    
    case 1: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "BASE STATION");
      display.setColor(WHITE);
      
      #ifdef BASE_STATION
        BaseStationConfig config = configStorage.getBaseStationConfig();
        String idLine = "ID: 0  Net: " + String(config.networkId);
        if (securityManager.isEncryptionEnabled()) {
          idLine += " [E]";
        }
        display.drawString(0, 12, idLine);
      #endif
      
      bool wifiConnected = (WiFi.status() == WL_CONNECTED);
      display.drawString(0, 24, "WiFi: " + String(wifiConnected ? "Connected" : "Off"));
      drawWifiStatus(wifiConnected, 110, 24);
      
      uint8_t clientCount = getActiveClientCount();
      display.drawString(0, 36, "Clients: " + String(clientCount));
      
      if (stats->lastRxTime > 0) {
        uint32_t secAgo = (millis() - stats->lastRxTime) / 1000;
        display.drawString(0, 48, "Last RX: " + String(secAgo) + "s");
      } else {
        display.drawString(0, 48, "Last RX: Never");
      }
      
      display.drawString(110, 54, "2/8");
      break;
    }
    
    case 2: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "SENSOR SUMMARY");
      display.setColor(WHITE);
      
      uint8_t activeCount = getActiveClientCount();
      uint8_t totalCount = getActiveSensorCount();
      
      display.drawString(0, 14, "Active: " + String(activeCount));
      display.drawString(0, 26, "Total seen: " + String(totalCount));
      
      // Show oldest sensor last seen time
      if (totalCount > 0) {
        uint32_t oldestTime = 0;
        for (uint8_t i = 0; i < totalCount; i++) {
          SensorInfo* sensor = getSensorByIndex(i);
          if (sensor) {
            uint32_t secAgo = (millis() - sensor->lastSeen) / 1000;
            if (secAgo > oldestTime) oldestTime = secAgo;
          }
        }
        display.drawString(0, 38, "Oldest: " + String(oldestTime) + "s ago");
      }
      
      display.drawString(110, 54, "3/8");
      break;
    }
    
    case 3: {
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
      
      display.drawString(110, 54, "4/8");
      break;
    }
    
    case 4: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "LORA CONFIG");
      display.setColor(WHITE);
      
      // Read current LoRa parameters from NVS or use defaults
      Preferences prefs;
      prefs.begin("lora_params", true);  // Read-only
      uint32_t frequency = prefs.getUInt("frequency", RF_FREQUENCY);
      uint8_t spreadingFactor = prefs.getUChar("sf", LORA_SPREADING_FACTOR);
      uint32_t bandwidth = prefs.getUInt("bandwidth", LORA_BANDWIDTH);
      uint8_t txPower = prefs.getUChar("tx_power", TX_OUTPUT_POWER);
      uint8_t codingRate = prefs.getUChar("coding_rate", LORA_CODINGRATE);
      prefs.end();
      
      // Display frequency in MHz
      float freqMHz = frequency / 1000000.0;
      display.drawString(0, 12, "Freq: " + String(freqMHz, 1) + " MHz");
      
      // Display spreading factor
      display.drawString(0, 24, "SF: " + String(spreadingFactor) + "  CR: 4/" + String(codingRate + 4));
      
      // Display bandwidth in kHz
      uint16_t bwKHz = bandwidth / 1000;
      display.drawString(0, 36, "BW: " + String(bwKHz) + " kHz");
      
      // Display TX power
      display.drawString(0, 48, "Power: " + String(txPower) + " dBm");
      
      display.drawString(110, 54, "5/8");
      break;
    }
    
    case 5: {
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
        
        display.drawString(0, 20, "Voltage: " + String(batteryVoltage, 2) + "V");
        display.drawString(0, 32, "Level: " + String(batteryPercent) + "%");
        drawBatteryIcon(batteryPercent, 90, 32);
        display.drawString(0, 48, String(powerState ? "Charging" : "Discharging"));
      #endif
      
      display.drawString(110, 54, "6/8");
      break;
    }
    
    case 6: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "WIFI INFO");
      display.setColor(WHITE);
      
      bool wifiConnected = (WiFi.status() == WL_CONNECTED);
      
      if (wifiConnected) {
        // Display IP address
        String ipStr = WiFi.localIP().toString();
        display.drawString(0, 14, "IP: " + ipStr);
        
        // Display WiFi SSID (truncate if too long)
        String ssid = WiFi.SSID();
        if (ssid.length() > 16) {
          ssid = ssid.substring(0, 13) + "...";
        }
        display.drawString(0, 26, "SSID: " + ssid);
        
        // Display WiFi uptime in dynamic format
        uint32_t uptimeSec = millis() / 1000;
        String uptimeStr;
        if (uptimeSec < 60) {
          uptimeStr = String(uptimeSec) + "s";
        } else if (uptimeSec < 3600) {
          uptimeStr = String(uptimeSec / 60) + "m";
        } else if (uptimeSec < 86400) {
          uptimeStr = String(uptimeSec / 3600) + "h";
        } else {
          uptimeStr = String(uptimeSec / 86400) + "d";
        }
        display.drawString(0, 38, "Uptime: " + uptimeStr);
        
        // Display signal strength (RSSI)
        int32_t rssi = WiFi.RSSI();
        display.drawString(0, 50, "RSSI: " + String(rssi) + " dBm");
        
        // Display WiFi status icon
        drawWifiStatus(true, 110, 38);
      } else {
        display.drawString(0, 20, "WiFi: Not");
        display.drawString(0, 32, "Connected");
        drawWifiStatus(false, 110, 38);
      }
      
      display.drawString(110, 54, "7/8");
      break;
    }

    case 7: {
      // TIME & NTP page
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "TIME & NTP");
      display.setColor(WHITE);

      // Current local time
      time_t now = time(nullptr);
      if (now > 1000) {
        struct tm tmnow;
        localtime_r(&now, &tmnow);
        char buf1[20];
        strftime(buf1, sizeof(buf1), "%H:%M:%S", &tmnow);
        char buf2[20];
        strftime(buf2, sizeof(buf2), "%Y-%m-%d", &tmnow);
        display.drawString(0, 12, String("Now: ") + buf1);
        display.drawString(0, 24, String("Date: ") + buf2);
      } else {
        display.drawString(0, 12, "Now: --:--:--");
      }

      NTPConfig cfg = configStorage.getNTPConfig();
      String ntpLine = String("NTP: ") + (cfg.enabled ? "ON" : "OFF");
      if (cfg.enabled) {
        String server = String(cfg.server);
        if (server.length() > 10) server = server.substring(0, 10) + "...";
        ntpLine += String(" ") + server;
      }
      display.drawString(0, 36, ntpLine);

      // Last syncs
      time_t lastNtp = getLastNtpSyncEpoch();
      uint32_t lastBc = getLastTimeBroadcastMs();
      String line = "Sync: ";
      if (lastNtp > 0) {
        uint32_t ago = (uint32_t)difftime(time(nullptr), lastNtp);
        line += String(ago/60) + "m";
      } else {
        line += "--";
      }
      line += " / BC: ";
      if (lastBc > 0) {
        uint32_t agoMs = millis() - lastBc;
        line += String((agoMs/1000)/60) + "m";
      } else {
        line += "--";
      }
      display.drawString(0, 48, line);

      display.drawString(110, 54, "8/8");
      break;
    }
  }
  
  // Draw command notification overlay (if active)
  updateCommandNotification();
  
  display.display();
}
#endif // BASE_STATION

// Sensor display pages
#ifdef SENSOR_NODE
void displaySensorPage() {
  if (!displayOn) return;
  
  display.clear();
  SystemStats* stats = getStats();
  
  switch (currentPage) {
    case 0: {
      // Welcome page
      SensorConfig config = configStorage.getSensorConfig();
      String location = String(config.location);
      
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      
      display.drawString(64, 15, "Hello! I am");
      
      // Use smaller font for long names to fit more characters
      if (location.length() > 14) {
        display.setFont(ArialMT_Plain_10);
        if (location.length() > 20) {
          location = location.substring(0, 20);
        }
        display.drawString(64, 38, location);
      } else {
        display.drawString(64, 35, location);
      }
      
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_10);
      display.drawString(110, 54, "1/6");
      break;
    }
    
    case 1: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "CLIENT STATUS");
      display.setColor(WHITE);
      
      SensorConfig config = configStorage.getSensorConfig();
      
      // Display sensor ID and network ID on same line
      String idLine = "ID: " + String(config.sensorId) + "  Net: " + String(config.networkId);
      if (securityManager.isEncryptionEnabled()) {
        idLine += " [E]";
      }
      display.drawString(0, 12, idLine);
      
      // Display transmit interval
      display.drawString(0, 24, "Interval: " + String(config.transmitInterval) + "s");
      
      // Display uptime
      uint32_t uptime = millis() / 1000;
      display.drawString(0, 36, "Uptime: " + String(uptime) + "s");
      
      if (stats->lastTxTime > 0) {
        uint32_t secAgo = (millis() - stats->lastTxTime) / 1000;
        display.drawString(0, 48, "Last TX: " + String(secAgo) + "s");
      }
      
      display.drawString(110, 54, "2/6");
      break;
    }
    
    case 2: {
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
      
      display.drawString(110, 54, "3/6");
      break;
    }
    
    case 3: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "BATTERY STATUS");
      display.setColor(WHITE);
      
      display.drawString(0, 20, "Voltage: N/A");
      display.drawString(0, 32, "Level: N/A");
      drawBatteryIcon(0, 90, 32);
      display.drawString(0, 48, "Connect battery");
      
      display.drawString(110, 54, "4/6");
      break;
    }
    
    case 4: {
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "LORA CONFIG");
      display.setColor(WHITE);
      
      // Read current LoRa parameters from NVS or use defaults
      Preferences prefs;
      prefs.begin("lora_params", true);  // Read-only
      uint32_t frequency = prefs.getUInt("frequency", RF_FREQUENCY);
      uint8_t spreadingFactor = prefs.getUChar("sf", LORA_SPREADING_FACTOR);
      uint32_t bandwidth = prefs.getUInt("bandwidth", LORA_BANDWIDTH);
      uint8_t txPower = prefs.getUChar("tx_power", TX_OUTPUT_POWER);
      uint8_t codingRate = prefs.getUChar("coding_rate", LORA_CODINGRATE);
      prefs.end();
      
      // Display frequency in MHz
      float freqMHz = frequency / 1000000.0;
      display.drawString(0, 12, "Freq: " + String(freqMHz, 1) + " MHz");
      
      // Display spreading factor
      display.drawString(0, 24, "SF: " + String(spreadingFactor) + "  CR: 4/" + String(codingRate + 4));
      
      // Display bandwidth in kHz
      uint16_t bwKHz = bandwidth / 1000;
      display.drawString(0, 36, "BW: " + String(bwKHz) + " kHz");
      
      // Display TX power
      display.drawString(0, 48, "Power: " + String(txPower) + " dBm");
      
      display.drawString(110, 54, "5/6");
      break;
    }

    case 5: {
      // Sensor TIME & SYNC page
      display.setFont(ArialMT_Plain_10);
      display.setColor(WHITE);
      display.fillRect(0, 0, 128, 11);
      display.setColor(BLACK);
      display.drawString(0, 0, "TIME SYNC");
      display.setColor(WHITE);

      time_t now = time(nullptr);
      if (now > 1000000000) {  // Valid Unix timestamp (after year 2001)
        struct tm tmnow;
        localtime_r(&now, &tmnow);
        char buf1[20];
        strftime(buf1, sizeof(buf1), "%H:%M:%S", &tmnow);
        char buf2[20];
        strftime(buf2, sizeof(buf2), "%Y-%m-%d", &tmnow);
        display.drawString(0, 14, String("Now: ") + buf1);
        display.drawString(0, 26, String("Date: ") + buf2);
      } else {
        display.drawString(0, 14, "Now: --:--:--");
        display.drawString(0, 26, "Date: Not synced");
      }

      uint32_t lastEpoch = getSensorLastTimeSyncEpoch();
      if (lastEpoch > 0) {
        time_t te = (time_t)lastEpoch;
        uint32_t ago = (uint32_t)difftime(time(nullptr), te);
        display.drawString(0, 40, String("Last Sync: ") + String(ago/60) + "m ago");
      } else {
        display.drawString(0, 40, "Last Sync: --");
      }

      display.drawString(110, 54, "6/6");
      break;
    }
  }
  
  // Draw command notification overlay (if active)
  updateCommandNotification();
  
  display.display();
}
#endif // SENSOR_NODE

// Show command received notification (sensor nodes)
void showCommandNotification() {
  #ifdef SENSOR_NODE
  showingCommandNotif = true;
  commandNotifStartTime = millis();
  
  if (!displayOn) {
    wakeDisplay();
  }

  // Render immediately so the user sees it right away, even if the main loop
  // is busy after command processing.
  display.clear();
  const int boxWidth = 90;
  const int boxHeight = 20;
  const int boxX = (128 - boxWidth) / 2; // Center horizontally
  const int boxY = (64 - boxHeight) / 2; // Center vertically
  
  // Draw filled rectangle as background
  display.setColor(BLACK);
  display.fillRect(boxX, boxY, boxWidth, boxHeight);
  
  // Draw border
  display.setColor(WHITE);
  display.drawRect(boxX, boxY, boxWidth, boxHeight);
  display.drawRect(boxX + 1, boxY + 1, boxWidth - 2, boxHeight - 2);
  
  // Draw text centered
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, boxY + 5, "Cmd Recv'd");
  display.setTextAlignment(TEXT_ALIGN_LEFT); // Reset alignment
  display.display();
  #endif
}

// Update command notification - call this in display loop
void updateCommandNotification() {
  #ifdef SENSOR_NODE
  if (showingCommandNotif) {
    // Check if notification should be dismissed
    if (millis() - commandNotifStartTime >= COMMAND_NOTIF_DURATION) {
      showingCommandNotif = false;
      return;
    }
    
    // Draw notification box overlay
    const int boxWidth = 90;
    const int boxHeight = 20;
    const int boxX = (128 - boxWidth) / 2; // Center horizontally
    const int boxY = (64 - boxHeight) / 2; // Center vertically
    
    // Draw filled rectangle as background
    display.setColor(BLACK);
    display.fillRect(boxX, boxY, boxWidth, boxHeight);
    
    // Draw border
    display.setColor(WHITE);
    display.drawRect(boxX, boxY, boxWidth, boxHeight);
    display.drawRect(boxX + 1, boxY + 1, boxWidth - 2, boxHeight - 2);
    
    // Draw text centered
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, boxY + 5, "Cmd Recv'd");
    display.setTextAlignment(TEXT_ALIGN_LEFT); // Reset alignment
  }
  #endif
}
