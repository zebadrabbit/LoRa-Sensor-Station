# LoRa Sensor Station

A modular IoT sensor network built on Heltec WiFi LoRa 32 V3 boards, enabling remote temperature and battery monitoring with multi-page cycling displays and comprehensive statistics tracking.

## Overview

This project implements a LoRa-based sensor network with:
- **Base Station**: Receives and displays data from multiple remote sensors
- **Sensor Nodes**: Transmit temperature and battery status at regular intervals

Both devices feature OLED displays with automatic page cycling and WS2812 LED indicators.

## Hardware

- **Board**: Heltec WiFi LoRa 32 V3 (ESP32-S3)
- **Radio**: SX1262 LoRa transceiver
- **Display**: 128x64 OLED (SSD1306)
- **LED**: WS2812 NeoPixel (GPIO48)
- **Sensors**: Thermistor (10kΩ NTC) for temperature
- **Power**: Battery monitoring via ADC

## Features

### Base Station
- **4-Page Cycling Display** (5-second intervals):
  1. Status: WiFi connectivity, active sensor count, last RX time
  2. Sensor List: Up to 4 sensors with temp/battery/age
  3. Statistics: RX packets, invalid packets, success rate
  4. Signal Graph: RSSI history visualization
- Tracks up to 10 sensors simultaneously
- LED color indication based on sensor battery levels
- 5-minute display timeout with button wake

### Sensor Nodes
- **3-Page Cycling Display** (5-second intervals):
  1. Status: Uptime, last transmission time
  2. TX Statistics: Attempts/success/failures/rate
  3. Battery: Battery level with icon
- Automatic transmission every 30 seconds
- LED feedback on transmission status
- 5-minute display timeout with button wake

## Software Architecture

### Modular Structure
```
include/
├── config.h              # Configuration constants
├── data_types.h          # Data structures and packet format
├── led_control.h         # WS2812 LED interface
├── display_control.h     # OLED display management
├── sensor_readings.h     # ADC and sensor interfaces
├── lora_comm.h           # LoRa communication
└── statistics.h          # Statistics tracking

src/
├── main.cpp              # Main program (97 lines)
├── data_types.cpp        # Checksum utilities
├── led_control.cpp       # LED control implementation
├── display_control.cpp   # Multi-page display logic
├── sensor_readings.cpp   # Thermistor and battery reading
├── lora_comm.cpp         # Radio initialization and callbacks
└── statistics.cpp        # TX/RX statistics tracking
```

## Radio Configuration

- **Frequency**: 868 MHz (EU868)
- **Spreading Factor**: SF7
- **Bandwidth**: 125 kHz
- **Coding Rate**: 4/5
- **Packet Size**: 16 bytes
- **Sync Word**: 0xAB
- **Preamble**: 8 symbols

## Packet Structure

```c
struct SensorData {
    uint8_t syncWord;        // Packet sync (0xAB)
    uint8_t sensorId;        // Unique sensor ID
    float temperature;       // Temperature in °C
    float batteryVoltage;    // Battery voltage
    uint8_t batteryPercent;  // Battery percentage
    bool powerState;         // Charging state
    uint8_t checksum;        // XOR checksum
} __attribute__((packed));
```

## Building and Uploading

### Prerequisites
- [PlatformIO](https://platformio.org/)
- USB drivers for ESP32-S3

### Compile
```bash
# Build base station
pio run -e base_station

# Build sensor
pio run -e sensor
```

### Upload
```bash
# Upload to base station (COM8)
pio run -e base_station -t upload

# Upload to sensor (COM11)
pio run -e sensor -t upload
```

### Monitor Serial Output
```bash
# Monitor base station
pio device monitor -p COM8 -b 115200

# Monitor sensor
pio device monitor -p COM11 -b 115200
```

## Configuration

Edit `include/config.h` to customize:

```cpp
// Radio Parameters
#define RF_FREQUENCY           868000000  // 868 MHz
#define LORA_SPREADING_FACTOR  7
#define LORA_BANDWIDTH         0          // 125 kHz

// Sensor Configuration
#define SENSOR_ID              1          // Unique sensor ID
#define SENSOR_INTERVAL        30000      // 30 seconds

// Display Settings
#define DISPLAY_TIMEOUT_MS     300000     // 5 minutes
#define DISPLAY_PAGE_CYCLE_MS  5000       // 5 seconds
```

## Pin Assignments

### Radio (SX1262)
- NSS: GPIO 8
- RESET: GPIO 12
- BUSY: GPIO 13
- DIO1: GPIO 14

### Display (OLED)
- SDA: GPIO 41
- SCL: GPIO 42
- VEXT: GPIO 36 (power control)

### LED (WS2812)
- Data: GPIO 48

### Sensors
- Thermistor: GPIO 1 (ADC)
- Battery: GPIO 1 (ADC with voltage divider)

### User Interface
- Button: GPIO 0 (USER button)

## Statistics Tracking

The system tracks:
- **TX Statistics**: Attempts, successes, failures, success rate
- **RX Statistics**: Total packets, invalid packets, success rate
- **Sensor Info**: Last seen time, RSSI, SNR, packet count, last readings
- **Signal History**: 50-sample RSSI ring buffer for graphing

## Display Timeout

Both devices implement a 5-minute display timeout:
- Display automatically turns off after 5 minutes of inactivity
- Press the USER button to wake the display
- Button press also cycles to the next page when display is active

## LED Color Codes

### Base Station (based on received sensor battery):
- **Green**: Battery > 80%
- **Yellow**: Battery 50-80%
- **Orange**: Battery 20-50%
- **Red**: Battery < 20%

### Sensor Node:
- **Blue blink**: Transmission successful
- **Red blink**: Transmission failed
- **Green/Yellow/Orange/Red steady**: Battery level indicator

## Troubleshooting

### No Communication
1. Verify both devices are on the same frequency (868 MHz)
2. Check antenna connections
3. Confirm SYNC_WORD matches (0xAB)
4. Review serial output for transmission logs

### Display Issues
1. Check Vext power control (GPIO 36)
2. Verify I2C connections (SDA/SCL)
3. Press USER button to wake from timeout

### Sensor Readings
1. Thermistor should be 10kΩ NTC
2. Battery ADC may need calibration
3. Check ADC pin configuration (GPIO 1)

## Future Enhancements

- [ ] WiFi connectivity for cloud data logging
- [ ] Teams/Email notifications for critical alerts
- [ ] Multi-sensor support with unique IDs
- [ ] Deep sleep mode for extended battery life
- [ ] SD card logging
- [ ] Web interface for configuration

## License

This project is open source. Feel free to modify and distribute.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Author

Created for IoT sensor monitoring applications using Heltec LoRa V3 hardware.

---

**Last Updated**: December 2025
