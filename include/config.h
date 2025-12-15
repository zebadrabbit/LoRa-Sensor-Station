#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// BOARD CONFIGURATION
// ============================================================================
#ifndef LED_PIN
  #define LED_PIN 48  // Default LED pin for Heltec V3
#endif

#ifndef THERMISTOR_PIN
  #define THERMISTOR_PIN 1  // Default analog pin
#endif

#ifndef USER_BUTTON
  #define USER_BUTTON 0  // Built-in USER button on Heltec V3
#endif

// Display Configuration
#define DISPLAY_TIMEOUT_MS      300000  // 5 minutes in milliseconds
#define DISPLAY_PAGE_CYCLE_MS   10000   // 10 seconds per page

// WiFi Configuration (optional - leave empty to disable)
#define WIFI_SSID               ""      // Your WiFi SSID
#define WIFI_PASSWORD           ""      // Your WiFi password

// Statistics Configuration
#define MAX_SENSORS             10      // Maximum number of sensors to track
#define SIGNAL_HISTORY_SIZE     32      // Number of RSSI samples to graph

// ============================================================================
// LORA PARAMETERS
// ============================================================================
#define RF_FREQUENCY                868000000   // Hz (adjust for your region: 433, 868, 915 MHz)
#define TX_OUTPUT_POWER             14          // dBm
#define LORA_BANDWIDTH              0           // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz]
#define LORA_SPREADING_FACTOR       10          // [SF7..SF12] - SF10 needed for 200-byte packets
#define LORA_CODINGRATE             1           // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH        8           // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT         0           // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false

// Packet Configuration
#define SYNC_WORD                   0x1234

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================
#define SENSOR_ID                   1           // Change for each sensor
#define SENSOR_INTERVAL             30000       // 30 seconds between transmissions
#define BATTERY_SAMPLES             10          // Number of samples for battery average

// Thermistor Configuration (10K NTC thermistor with 10K series resistor)
#define THERMISTOR_NOMINAL          10000       // Nominal resistance at 25°C
#define TEMPERATURE_NOMINAL         25          // Temperature for nominal resistance
#define B_COEFFICIENT               3950        // Beta coefficient
#define SERIES_RESISTOR             10000       // Series resistor value

// ============================================================================
// WS2812 LED CONFIGURATION
// ============================================================================
#define NUM_LEDS                    1           // Number of LEDs

#endif // CONFIG_H
