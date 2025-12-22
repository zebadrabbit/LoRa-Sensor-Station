#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include "data_types.h"
#include "statistics.h"
#include "sensor_readings.h"

// Initialize display system
void initDisplay();

// Display timeout management
void updateDisplayTimeout();
bool isDisplayOn();
void wakeDisplay();
void checkDisplayTimeout();

// Button handling with multi-click detection
void handleButton();
bool shouldSendImmediatePing();
void clearImmediatePingFlag();

// Display cycling
void cycleDisplayPages();
void forceNextPage();

// Display functions for base station
void displayBaseStationPage();

// Display functions for sensor
void displaySensorPage();

// Simple message display (for setup/errors)
void displayMessage(const char* line1, const char* line2, const char* line3, uint16_t duration);

// QR code display for captive portal
void displayQRCode(const char* url);

// Graphics utilities
void drawSignalGraph(int16_t* rssiHistory, uint8_t size, int x, int y, int width, int height);
void drawBatteryIcon(uint8_t percent, int x, int y);
void drawWifiStatus(bool connected, int x, int y);

// Command notification
void showCommandNotification();
void updateCommandNotification();

#endif // DISPLAY_CONTROL_H
