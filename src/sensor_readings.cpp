#include "sensor_readings.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

void initSensors() {
  #ifdef SENSOR_NODE
    // Configure ADC for battery and thermistor readings
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // For 0-3.3V range
  #endif
}

float readThermistor() {
  #ifdef SENSOR_NODE
    // Read thermistor multiple times and average
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) {
      sum += analogRead(THERMISTOR_PIN);
      delay(10);
    }
    float average = sum / 10.0;
    
    // Convert to resistance
    float resistance = SERIES_RESISTOR / ((4095.0 / average) - 1.0);
    
    // Steinhart-Hart equation (simplified)
    float steinhart;
    steinhart = resistance / THERMISTOR_NOMINAL;           // (R/Ro)
    steinhart = log(steinhart);                            // ln(R/Ro)
    steinhart /= B_COEFFICIENT;                            // 1/B * ln(R/Ro)
    steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);     // + (1/To)
    steinhart = 1.0 / steinhart;                           // Invert
    steinhart -= 273.15;                                   // Convert to C
    
    return steinhart;
  #else
    return 0.0;
  #endif
}

float readBatteryVoltage() {
  // Heltec V3 has battery voltage on ADC1_CH0 (GPIO1) with voltage divider
  // The voltage divider is typically 1:1, so we need to multiply by 2
  // ADC reference is 3.3V, 12-bit (4096 steps)
  
  uint32_t sum = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    sum += analogRead(1);  // GPIO1 is battery voltage pin
    delay(10);
  }
  float average = sum / (float)BATTERY_SAMPLES;
  float voltage = (average / 4095.0) * 3.3 * 2.0;  // Multiply by 2 for voltage divider
  
  return voltage;
}

uint8_t calculateBatteryPercent(float voltage) {
  // LiPo battery voltage range: 4.2V (full) to 3.0V (empty)
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.0) return 0;
  
  // Linear approximation
  float percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100.0;
  return (uint8_t)constrain(percent, 0, 100);
}

bool getPowerState() {
  // Check if battery voltage is increasing (charging)
  // For simplicity, we'll check if voltage is above 4.1V (likely charging)
  float voltage = readBatteryVoltage();
  return voltage > 4.1;
}
