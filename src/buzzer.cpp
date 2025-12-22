#include "buzzer.h"
#include <Arduino.h>
#include <esp_timer.h>

namespace {
// Arduino-ESP32 (2.x) LEDC API
constexpr uint8_t kDefaultChannel = 7;
constexpr uint8_t kResolutionBits = 8;

uint8_t buzzerPin = 7;
bool initialized = false;

const BuzzerStep* current = nullptr;
uint8_t currentCount = 0;
uint8_t stepIndex = 0;
bool playing = false;

esp_timer_handle_t stepTimer = nullptr;
portMUX_TYPE buzzerMux = portMUX_INITIALIZER_UNLOCKED;

void applyStep(const BuzzerStep& step) {
  if (step.freqHz == 0) {
    // Silence
    ledcWriteTone(kDefaultChannel, 0);
    ledcWrite(kDefaultChannel, 0);
  } else {
    ledcWriteTone(kDefaultChannel, step.freqHz);
    // Small duty to avoid being overly loud; piezos are capacitive.
    ledcWrite(kDefaultChannel, 128);
  }
}

void stopTimerLocked() {
  if (stepTimer != nullptr) {
    esp_timer_stop(stepTimer);
  }
}

void scheduleNextStepLocked(uint32_t durationMs) {
  stopTimerLocked();
  if (stepTimer == nullptr || durationMs == 0) {
    return;
  }
  // Clamp to short tones so we can't produce obnoxiously long beeps even if misused.
  if (durationMs > 250) {
    durationMs = 250;
  }
  esp_timer_start_once(stepTimer, (uint64_t)durationMs * 1000ULL);
}

void IRAM_ATTR onStepTimer(void* /*arg*/) {
  // Runs in esp_timer task context (not ISR), but keep critical sections tight.
  portENTER_CRITICAL(&buzzerMux);

  if (!playing || current == nullptr || currentCount == 0) {
    stopTimerLocked();
    portEXIT_CRITICAL(&buzzerMux);
    return;
  }

  // Advance to next step
  stepIndex++;
  if (stepIndex >= currentCount) {
    // End of provided list
    playing = false;
    current = nullptr;
    currentCount = 0;
    stepIndex = 0;
    stopTimerLocked();
    portEXIT_CRITICAL(&buzzerMux);
    ledcWriteTone(kDefaultChannel, 0);
    ledcWrite(kDefaultChannel, 0);
    return;
  }

  const BuzzerStep step = current[stepIndex];
  if (step.durationMs == 0) {
    // End sentinel
    playing = false;
    current = nullptr;
    currentCount = 0;
    stepIndex = 0;
    stopTimerLocked();
    portEXIT_CRITICAL(&buzzerMux);
    ledcWriteTone(kDefaultChannel, 0);
    ledcWrite(kDefaultChannel, 0);
    return;
  }

  // Schedule next transition, then apply this step outside the critical section.
  scheduleNextStepLocked(step.durationMs);
  portEXIT_CRITICAL(&buzzerMux);
  applyStep(step);
}

// Two-tone chirp: short + bright.
constexpr BuzzerStep kCmdReceived[] = {
    {880, 60},
    {0, 20},
    {1175, 90},
    {0, 0},
};

// 3 climbing chimes for startup.
constexpr BuzzerStep kStartupChime[] = {
  {784, 90},
  {0, 30},
  {988, 90},
  {0, 30},
  {1319, 120},
  {0, 0},
};
} // namespace

void buzzerInit(uint8_t gpioPin) {
  buzzerPin = gpioPin;

  if (!initialized) {
    ledcSetup(kDefaultChannel, 2000 /*dummy*/, kResolutionBits);
    ledcAttachPin(buzzerPin, kDefaultChannel);

    if (stepTimer == nullptr) {
      esp_timer_create_args_t args{};
      args.callback = &onStepTimer;
      args.arg = nullptr;
      args.dispatch_method = ESP_TIMER_TASK;
      args.name = "buzzer";
      esp_timer_create(&args, &stepTimer);
    }

    initialized = true;
  }

  buzzerStop();
}

void buzzerPlayPattern(const BuzzerStep* steps, uint8_t stepCount) {
  if (!initialized || steps == nullptr || stepCount == 0) {
    return;
  }

  portENTER_CRITICAL(&buzzerMux);
  current = steps;
  currentCount = stepCount;
  stepIndex = 0;
  playing = true;

  const BuzzerStep step0 = current[0];
  if (step0.durationMs == 0) {
    playing = false;
    current = nullptr;
    currentCount = 0;
    stepIndex = 0;
    stopTimerLocked();
    portEXIT_CRITICAL(&buzzerMux);
    buzzerStop();
    return;
  }

  scheduleNextStepLocked(step0.durationMs);
  portEXIT_CRITICAL(&buzzerMux);
  applyStep(step0);
}

void buzzerPlayCommandReceived() {
  buzzerPlayPattern(kCmdReceived, sizeof(kCmdReceived) / sizeof(kCmdReceived[0]));
}

void buzzerPlayStartupChime() {
  buzzerPlayPattern(kStartupChime, sizeof(kStartupChime) / sizeof(kStartupChime[0]));
}

void buzzerStop() {
  portENTER_CRITICAL(&buzzerMux);
  playing = false;
  current = nullptr;
  currentCount = 0;
  stepIndex = 0;
  stopTimerLocked();
  portEXIT_CRITICAL(&buzzerMux);
  ledcWriteTone(kDefaultChannel, 0);
  ledcWrite(kDefaultChannel, 0);
}

void buzzerTick() {
  // Intentionally empty: patterns are advanced by esp_timer.
}
