#include "led_control.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void initLED() {
  strip.begin();
  strip.setBrightness(50);  // 0-255
  strip.show();
}

void setLED(uint32_t color, uint8_t brightness) {
  strip.setBrightness(brightness);
  strip.setPixelColor(0, color);
  strip.show();
}

void setLEDColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness) {
  strip.setBrightness(brightness);
  strip.setPixelColor(0, strip.Color(red, green, blue));
  strip.show();
}

void blinkLED(uint32_t color, int times, int delayMs) {
  uint8_t originalBrightness = strip.getBrightness();
  for (int i = 0; i < times; i++) {
    setLED(color, originalBrightness);
    delay(delayMs);
    setLED(getColorOff(), originalBrightness);
    delay(delayMs);
  }
}

// Color definitions
uint32_t getColorOff()    { return strip.Color(0, 0, 0); }
uint32_t getColorGreen()  { return strip.Color(0, 255, 0); }
uint32_t getColorYellow() { return strip.Color(255, 255, 0); }
uint32_t getColorOrange() { return strip.Color(255, 165, 0); }
uint32_t getColorRed()    { return strip.Color(255, 0, 0); }
uint32_t getColorBlue()   { return strip.Color(0, 0, 255); }
uint32_t getColorPurple() { return strip.Color(128, 0, 128); }
