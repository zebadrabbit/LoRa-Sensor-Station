#ifndef BUZZER_H
#define BUZZER_H

#include "config.h"
#include <stdint.h>

// Piezo buzzer driver (LEDC PWM)
// Notes:
// - Patterns are advanced/stopped via esp_timer, so a stalled main loop won't leave the buzzer stuck ON.
// - buzzerTick() is retained for backward compatibility but is not required.

struct BuzzerStep {
  uint16_t freqHz;      // 0 = silence
  uint16_t durationMs;  // how long to hold this step
};

// Initialize buzzer output. Safe to call multiple times.
void buzzerInit(uint8_t gpioPin = BUZZER_PIN);

// Backward-compatible no-op (patterns are timer-driven now).
void buzzerTick();

// Immediately stop any sound/pattern.
void buzzerStop();

// Play a short "happy" sound for command received.
void buzzerPlayCommandReceived();

// Play a short 3-note ascending startup chime.
void buzzerPlayStartupChime();

// Optional generic pattern API.
void buzzerPlayPattern(const BuzzerStep* steps, uint8_t stepCount);

#endif // BUZZER_H
