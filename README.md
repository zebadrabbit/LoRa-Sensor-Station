# LoRa Sensor Station

A modular IoT sensor network built on Heltec WiFi LoRa 32 V3 boards with **zero-configuration WiFi captive portal**, enabling remote temperature and battery monitoring with multi-page cycling displays and comprehensive statistics tracking.

## Overview

This project implements a LoRa-based sensor network with:
- **Base Station**: Receives and displays data from multiple remote sensors, with WiFi connectivity
- **Sensor Nodes**: Transmit temperature and battery status at configurable intervals
- **WiFi Captive Portal**: Easy setup without hardcoded credentials
- **Dynamic Configuration**: All settings configurable via web interface
- **Advanced Button Controls**: Multi-click detection with immediate ping functionality
- **Sensor Health Monitoring**: Automatic timeout detection and alerting

Both devices feature OLED displays with automatic page cycling, inverse headers, and WS2812 LED indicators.

## Hardware

- **Board**: Heltec WiFi LoRa 32 V3 (ESP32-S3)
- **Radio**: SX1262 LoRa transceiver
- **Display**: 128x64 OLED (SSD1306)
- **LED**: WS2812 NeoPixel (GPIO48)
- **Sensors**: Thermistor (10kΩ NTC) for temperature
- **Power**: Battery monitoring via ADC

## Features

### 🆕 WiFi Captive Portal (v2.0)
- **Zero-configuration setup**: No hardcoded WiFi credentials required
- **First-boot automatic AP mode**: Device creates WiFi access point on first power-up
- **QR code display**: Scan to instantly connect (http://10.8.4.1)
- **Dual configuration paths**:
  - **Sensor Client**: Configure ID, location, transmit interval (15s/30s/60s/300s)
  - **Base Station**: Select WiFi network, enter password, test connection
- **Persistent storage**: All settings saved to ESP32 NVS
- **Factory reset**: 5-second button hold clears config and restarts setup
- **AP credentials**: SSID "LoRa-Sensor-XXXX" or "LoRa-Base-XXXX", password "configure"

### 🆕 Advanced Button Controls (v2.0)
- **Single click** (screen off): Wake display
- **Single click** (screen on): Cycle to next page
- **Double click**: Reboot device
- **Triple click**: Send immediate ping (sensors only)
- **5-second hold**: Factory reset

### 🆕 Sensor Health Monitoring (v2.0)
- **Automatic timeout detection**: Base station monitors sensor activity
- **Configurable thresholds**: 15-minute timeout (3x longest interval)
- **Periodic health checks**: Every 30 seconds
- **Serial warnings**: Logs when sensors go offline
- **Future-ready**: API for alert integration

### Base Station
- **5-Page Cycling Display** (5-second intervals):
  1. Status: WiFi connectivity, active sensor count, last RX time
  2. Sensor List: Up to 4 sensors with temp/battery/age
  3. Statistics: RX packets, invalid packets, success rate
  4. Signal Graph: RSSI history visualization
  5. **Battery Status** (NEW): Voltage, level, charging state for backup power
- Tracks up to 10 sensors simultaneously
- LED color indication based on sensor battery levels
- 5-minute display timeout with button wake
- **Inverse headers**: White-on-black title bars for all pages

### Sensor Nodes
- **3-Page Cycling Display** (5-second intervals):
  1. Status: Uptime, last transmission time
  2. TX Statistics: Attempts/success/failures/rate
  3. Battery: Battery level with icon
- **Configurable transmission intervals**: User-selectable via portal
- LED feedback on transmission status
- 5-minute display timeout with button wake
- **Inverse headers**: White-on-black title bars for all pages

## Configuration

### ⚙️ WiFi Captive Portal Setup (v2.0)

**First Boot (Unconfigured Devices):**
1. Power on the device
2. Display shows "First Boot!" and "Config Mode"
3. Device creates WiFi access point:
   - Sensor: "LoRa-Sensor-XXXX" (XXXX = last 4 hex of MAC)
   - Base Station: "LoRa-Base-XXXX"
   - Password: **configure**
4. QR code appears on display for instant connection
5. Connect to the WiFi network (or scan QR code)
6. Captive portal opens automatically at http://10.8.4.1

**Sensor Configuration:**
- Enter **Sensor ID** (1-255)
- Enter **Location** name
- Select **Transmit Interval**: 15s, 30s, 60s, or 300s (5min)
- Click "Configure Sensor"
- Device saves config, reboots, and begins operation

**Base Station Configuration:**
- Click "Scan for WiFi Networks"
- Select your network from the list
- Enter WiFi password (leave blank for open networks)
- Click "Test Connection" to verify
- Click "Configure Base Station"
- Device connects to WiFi and begins listening

**Factory Reset:**
- Hold PRG button for 5 seconds
- Display shows "Factory Reset!"
- Device clears all config and restarts in setup mode

### 📟 Display & Button Controls

**Button Actions:**
- **Single click** (screen off): Wake display
- **Single click** (screen on): Cycle to next page
- **Double click**: Reboot device
- **Triple click**: Send immediate ping (sensors only)
- **5-second hold**: Factory reset (clears all config)

**Display Pages:**
- Base Station: 5 pages (Status → Sensors → Statistics → Signal → Battery)
- Sensor: 3 pages (Status → TX Stats → Battery)
- Auto-advance every 5 seconds
- 5-minute timeout (press button to wake)

## Software Architecture

### Modular Structure
```
include/
├── config.h              # Configuration constants
├── config_storage.h      # NVS persistent storage
├── wifi_portal.h         # Captive portal web interface
├── data_types.h          # Data structures and packet format
├── led_control.h         # WS2812 LED interface
├── display_control.h     # OLED display + button control
├── sensor_readings.h     # ADC and sensor interfaces
├── lora_comm.h           # LoRa communication
└── statistics.h          # Statistics tracking + health monitoring

src/
├── main.cpp              # Main program with dynamic configuration
├── config_storage.cpp    # NVS read/write operations
├── wifi_portal.cpp       # Web server + captive portal logic
├── data_types.cpp        # Checksum utilities
├── led_control.cpp       # LED control implementation
├── display_control.cpp   # Multi-page display + multi-click buttons
├── sensor_readings.cpp   # Thermistor and battery reading
├── lora_comm.cpp         # Radio initialization and callbacks
└── statistics.cpp        # TX/RX statistics + sensor timeout detection
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

### First Build
```bash
# Build base station
pio run -e base_station

# Build sensor
pio run -e sensor
```

### Upload
```bash
# Upload to base station (adjust COM port)
pio run -e base_station -t upload

# Upload to sensor (adjust COM port)
pio run -e sensor -t upload
```

### Monitor Serial Output
```bash
# Monitor base station
pio device monitor -b 115200

# Monitor sensor
pio device monitor -b 115200
```

### First-Boot Configuration

After upload, each device will:
1. Boot into configuration mode (First Boot)
2. Create WiFi access point with QR code
3. Wait for user configuration via captive portal
4. Save settings to NVS and reboot
5. Operate normally until factory reset

**No code changes required** - all configuration is done through the web interface!

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
## Troubleshooting

### WiFi Connection Issues
1. Verify WiFi credentials entered correctly in portal
2. Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Try "Test Connection" before saving config
4. Check serial output for connection error messages
5. Factory reset (5s hold) to reconfigure

### Display Issues
1. Check Vext power control (GPIO 36)
2. Verify I2C connections (SDA/SCL)
3. Press USER button to wake from timeout

### Sensor Readings
1. Thermistor should be 10kΩ NTC
2. Battery ADC may need calibration
3. Check ADC pin configuration (GPIO 1)

### Captive Portal Not Opening
1. Disconnect from other WiFi networks first
2. Manually navigate to http://10.8.4.1
3. Check device is in "Config Mode" on display
4. Try scanning QR code with camera app
5. Some devices require "Stay connected" prompt acceptance

### Button Not Responding
1. Verify debounce settings (50ms)
2. Check GPIO 0 connection
3. Try different click speeds (multi-click timeout is 400ms)

## Future Enhancements (Phase 2)

- [ ] Web dashboard for base station with live charts
- [ ] Teams/Email/SMS notifications for sensor alerts
- [ ] Cloud data logging (InfluxDB, MQTT)
- [ ] OTA firmware updates via web interface
- [ ] Deep sleep mode for sensors (extended battery life)
- [ ] SD card logging on base station
- [ ] Mobile app for remote monitoring
- [ ] Multi-base-station mesh networking

## Version History

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

**Current Version**: 2.0.0 (December 2025)

## License

This project is open source. Feel free to modify and distribute.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Author

Created for IoT sensor monitoring applications using Heltec LoRa V3 hardware.

---

**Last Updated**: December 12, 2025
