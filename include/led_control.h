#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

// Initialize LED system
void initLED();

// LED control functions
void setLED(uint32_t color, uint8_t brightness = 255);
void blinkLED(uint32_t color, int times = 1, int delayMs = 100);

// LED color definitions
uint32_t getColorOff();
uint32_t getColorGreen();
uint32_t getColorYellow();
uint32_t getColorOrange();
uint32_t getColorRed();
uint32_t getColorBlue();
uint32_t getColorPurple();

#endif // LED_CONTROL_H
