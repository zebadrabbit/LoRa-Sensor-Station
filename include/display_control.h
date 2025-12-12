#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include "data_types.h"
#include "statistics.h"

// Initialize display system
void initDisplay();

// Display timeout management
void updateDisplayTimeout();
bool isDisplayOn();
void wakeDisplay();
void checkDisplayTimeout();

// Display cycling
void cycleDisplayPages();
void forceNextPage();

// Display functions for base station
void displayBaseStationPage();

// Display functions for sensor
void displaySensorPage();

// Graphics utilities
void drawSignalGraph(int16_t* rssiHistory, uint8_t size, int x, int y, int width, int height);
void drawBatteryIcon(uint8_t percent, int x, int y);
void drawWifiStatus(bool connected, int x, int y);

#endif // DISPLAY_CONTROL_H
