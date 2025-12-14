# Changelog

All notable changes to the LoRa Sensor Station project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.13.0] - 2025-12-14

### Added - Network Pairing Phase 2: Encryption & Whitelisting

#### Core Security Implementation

- **AES-128-CBC encryption**: Using ESP32 mbedTLS (hardware-accelerated)
- **Random IV generation**: 16-byte initialization vector per packet for enhanced security
- **HMAC authentication**: 8-byte HMAC for message integrity verification
- **Replay attack prevention**: Sequence numbers with 100-packet sliding window
- **Device whitelist**: Support for up to 32 whitelisted devices (NVS-persisted)
- **Backward compatibility**: Encrypted and unencrypted devices can coexist in same network
- **Zero external dependencies**: Uses ESP32 framework built-in mbedTLS (removed Crypto library)

#### Security Management Web UI

- **New security page**: `/security` route with comprehensive management interface
- **Security card on dashboard**: Direct link to security settings from main dashboard
- **Encryption key display**: View current 32-character hex encryption key
- **Key copy button**: One-click copy to clipboard for easy device configuration
- **Custom key input**: Manually set encryption key (32 hex characters)
- **Key generation**: Generate new random encryption keys with warning confirmation
- **Whitelist management**: Add/remove/clear devices with live device count (X/32)
- **Toggle switches**: Enable/disable encryption and whitelist independently
- **Status cards**: Real-time visual indicators (green=enabled, red=disabled)

#### Security API Endpoints (9 total)

1. **GET /api/security/config** - Retrieve encryption/whitelist status
2. **POST /api/security/config** - Update encryption/whitelist settings
3. **GET /api/security/whitelist** - List all whitelisted devices
4. **POST /api/security/whitelist** - Add device to whitelist
5. **DELETE /api/security/whitelist/{id}** - Remove specific device
6. **DELETE /api/security/whitelist** - Clear entire whitelist
7. **POST /api/security/generate-key** - Generate new random key (returns key in response)
8. **GET /api/security/key** - Retrieve current encryption key as hex string
9. **POST /api/security/key** - Set custom encryption key from hex string

#### Sensor Configuration Integration

- **Encryption key field**: Added optional 32-character hex key input to sensor captive portal
- **Sensor configuration handler**: Parses and validates encryption key during setup
- **Key validation**: Validates hex format (0-9, A-F) and length (32 characters)
- **Auto-enable encryption**: Automatically enables encryption when valid key provided
- **Empty key support**: Leave blank to disable encryption (backward compatible)
- **Help text**: Clear instructions for users on where to find base station key

#### Display Enhancements

- **Security indicator**: Shows `[E]` on sensor status page when encryption enabled
- **Display format**: "ID: 1  Net: 12345 [E]" for encrypted sensors
- **ASCII indicator**: Uses `[E]` instead of emoji for C++ string compatibility

#### Technical Implementation

- **SecurityManager class**: Singleton pattern with full encryption lifecycle management
- **EncryptedPacket structure**: Header with type (0xE0), sequence, HMAC, payload, IV
- **Packet type detection**: Automatic detection of encrypted vs plaintext packets
- **LoRa integration**: Transparent encryption in `sendSensorData()` and `OnRxDone()`
- **NVS persistence**: Security config saved with encryption key, whitelist, sequence counter
- **mbedTLS API**: `mbedtls_aes_crypt_cbc()` for encryption/decryption
- **Stack-allocated contexts**: No member variables, proper init/free per operation

### Changed

- **Compilation**: Removed external Crypto library dependency
- **Memory usage**: Base station 42.7% flash (1,427,061 bytes) / 33.2% RAM (108,736 bytes)
- **LittleFS**: Added security.html to filesystem (deployed via uploadfs)
- **Dashboard.html**: Added security card with lock icon and link
- **WiFi portal**: Enhanced sensor configuration page with encryption field

### Fixed

- **Security route**: Added explicit `/security` route handler (was missing, causing 500 error)
- **Emoji encoding**: Replaced UTF-8 lock emoji with ASCII `[E]` in C++ code (compilation fix)
- **Key management**: Generate-key endpoint now returns the new key in response
- **Hex validation**: Proper hex string parsing with error handling

### Tested

- **Encryption workflow**: Base station key generation → sensor configuration → encrypted communication
- **Key management**: View, copy, generate, and set custom keys
- **Whitelist**: Add/remove devices, enforce whitelist filtering
- **Backward compatibility**: Mixed networks (encrypted + unencrypted) work correctly
- **Display indicator**: [E] shows on encrypted sensors
- **Security UI**: All toggles, buttons, and API endpoints functional

## [2.12.2] - 2025-12-14

### Added - Display Enhancements & UX Improvements

#### Sensor Display Enhancement

- **Sensor identification display**: First status page (1/3) now shows sensor ID and network ID
- **Display format**: "ID: 123 Net: 12345" for easy identification
- **Header update**: Changed from "SENSOR #X" to "SENSOR STATUS"
- **Configuration-based**: Uses actual config values instead of hardcoded constants

#### UX Consistency

- **Dashboard width**: Standardized from 1400px to 800px (matches all config pages)
- **Alerts page width**: Standardized to 800px (was 900px)
- **Client configuration page**: Fixed titles and labels
  - Changed "🔧 Sensor Configuration" to "📝 Client Configuration"
  - Changed "Active Sensors" to "Active Clients"
  - Changed "← Dashboard" to "← Back"
  - Updated help text for consistency

#### Performance Optimization

- **Alerts page to LittleFS**: Moved 20KB alerts.html from embedded code to filesystem
- **Memory freed**: ~20KB firmware size reduction
- **Route change**: `/alerts` now serves from `LittleFS.send("/alerts.html")`
- **Maintenance benefit**: HTML can be edited without firmware recompilation

### Fixed - Bug Resolutions

#### JavaScript Errors

- **/sensors page**: Fixed syntax error in saveSensor() error handler (missing quote)
- **Character corruption**: Cleaned up multiple corrupted emojis and special characters
- **Degree symbols**: Fixed `&larr;°C` corruption that broke raw string literals
- **showMessage() calls**: Fixed corrupted prefixes in JavaScript alerts

#### Validation & Testing

- **Network Pairing Phase 1**: Fully tested and validated
- **Network isolation**: Confirmed different Network IDs reject packets correctly
- **All pages loading**: Verified dashboard, alerts, MQTT, sensors pages render correctly
- **Memory stability**: Base station 42.9% flash / 33.1% RAM (stable)

## [2.1.1] - 2025-12-12

### Added - WiFi Information Display Page

#### Base Station Display Enhancement

- **6th display page**: Added WiFi information page showing network details
- **IP address display**: Shows web dashboard IP address for easy access
- **SSID display**: Displays connected WiFi network name (truncated if > 16 chars)
- **Signal strength**: Real-time RSSI (signal strength in dBm)
- **Connection status**: WiFi connected/disconnected indicator
- **WiFi status icon**: Visual indicator of connection state
- **Page indicators updated**: All pages now show X/6 instead of X/5

## [2.1.0] - 2025-12-12

### Added - Web Dashboard & Real-Time Monitoring

#### Web Dashboard

- **Real-time sensor monitoring**: Live dashboard at http://[BASE_STATION_IP]/
- **Auto-refresh system**: Updates every 5 seconds using JavaScript polling
- **Responsive design**: Mobile-friendly layout with color-coded status indicators
- **Sensor cards**: Individual cards for each sensor with color-coded status (green/yellow/red)
- **Status indicators**: Green (online), Yellow (stale), Red (offline) based on last seen time
- **Battery monitoring**: Visual battery percentage display for each sensor
- **Last seen timestamps**: Shows time since last update for each sensor

#### API Endpoints

- **/api/sensors**: JSON endpoint for sensor data retrieval
- **/api/stats**: JSON endpoint for system statistics
- **Sensor information**: ID, location, temperature, battery level, last seen time
- **Timeout status**: API includes timeout status for each sensor

#### Data Export

- **/export/csv**: Download sensor data in CSV format
- **/export/json**: Download complete sensor data in JSON format
- **Filename formatting**: Timestamped exports (sensors_YYYYMMDD_HHMMSS.csv/json)
- **Complete data**: Exports include all sensor details and statistics

## [2.0.0] - 2025-12-12

### Added - WiFi Captive Portal & Configuration System

#### WiFi Captive Portal

- **Zero-configuration setup**: First boot automatically starts AP mode with captive portal
- **Dual-mode configuration**: Separate paths for Sensor Client and Base Station setup
- **QR Code display**: OLED shows scannable QR code (http://10.8.4.1) for instant portal access
- **Responsive web interface**: Mobile-friendly HTML with gradient styling
- **AP credentials**: Unique SSID per device (LoRa-Sensor-XXXX / LoRa-Base-XXXX), password: "configure"
- **Custom IP scheme**: Changed from 192.168.4.1 to 10.8.4.1 for better QR code scanning

#### Configuration Storage (NVS)

- **Persistent storage**: Device mode, sensor config, WiFi credentials saved to ESP32 NVS
- **Three device modes**: UNCONFIGURED, SENSOR, BASE_STATION
- **Sensor configuration**: Sensor ID (1-255), location name (31 chars), transmit interval (15/30/60/300s)
- **Base station configuration**: WiFi SSID, password with connection testing before save
- **First-boot detection**: Automatic portal startup on unconfigured devices
- **Factory reset**: 5-second button hold clears all config and restarts portal

#### Sensor Client Configuration

- **Sensor ID assignment**: Unique identifier (1-255) for each sensor
- **Location naming**: Human-readable location field (max 31 characters)
- **Dynamic transmit intervals**: User-selectable intervals (15s, 30s, 60s, 300s)
- **Web form validation**: Real-time validation with error messages
- **Configuration preview**: Shows settings before confirmation

#### Base Station Configuration

- **WiFi network scanning**: Displays available networks with signal strength (RSSI)
- **Password entry**: Support for WPA/WPA2 secured networks
- **Open network support**: Password field optional for open WiFi networks
- **Connection testing**: Validates WiFi credentials (20 attempts) before saving
- **Fallback to AP**: Failed connection returns to portal for reconfiguration
- **WiFi timeout mitigation**: Scan timeout set to 300ms to prevent ESP32 watchdog crashes

#### User Interface Enhancements

- **Portrait mode QR code**: Display rotates 90° for better QR code visibility
- **Inverse headers**: All display pages now have inverse (white on black) header bars
- **Improved layout**: QR code properly positioned with "AP Pass: configure" text
- **Clear button feedback**: Visual confirmation of factory reset process

### Added - Advanced Button Controls

#### Multi-Click Detection System

- **Single click** (screen off): Wake display
- **Single click** (screen on): Cycle to next page
- **Double click** (screen on): Reboot device
- **Triple click** (screen on): Send immediate ping (sensors only)
- **5-second hold** (screen on): Factory reset and return to AP mode
- **Debouncing**: 50ms debounce with 400ms multi-click detection window
- **Hold detection**: Distinguishes between quick clicks and long holds

### Added - Sensor Health Monitoring

#### Timeout Detection System

- **Automatic timeout tracking**: Base station monitors when each sensor last reported
- **Configurable timeout threshold**: 15 minutes (3x longest transmit interval)
- **Periodic checks**: Base station checks sensor health every 30 seconds
- **Serial logging**: Warnings logged when sensors time out
- **API for alerts**: `isSensorTimedOut()` function ready for future alert integration
- **Active sensor management**: Sensors automatically marked inactive after timeout

#### Immediate Ping Feature

- **Manual connectivity test**: Triple-click button triggers immediate transmission
- **Bypasses interval timer**: Sends data immediately regardless of schedule
- **Visual feedback**: "Sending Ping..." message on OLED display
- **Battery indication**: Shows current battery level during ping

### Added - Base Station Battery Monitoring

- **New battery status page**: 5th page added to base station display cycle (5/5)
- **Real-time monitoring**: Voltage, percentage, and charging status
- **Power outage readiness**: Monitors backup battery for emergency operation
- **Visual battery icon**: Graphical representation of battery level
- **Charging indicator**: Displays "Charging" or "Discharging" status

### Enhanced - Display System

#### Signal Graph Improvements

- **Boundary constraints**: Graph lines now properly stay within borders
- **Layout optimization**: Reduced graph height to prevent text overflow
- **Label positioning**: Adjusted Y-axis labels (moved from Y=56/57 to Y=52)
- **Page indicator fix**: Moved text to prevent overlap with page numbers
- **X-axis labels**: Shortened "-20dBm" to "-20" for better spacing

#### Display Page Updates

- **Base station pages**:
  1. Status (1/5) - WiFi, sensor count, last RX
  2. Active Sensors (2/5) - Up to 4 sensors with details
  3. Statistics (3/5) - RX packets, success rate
  4. Signal History (4/5) - RSSI graph
  5. **Battery Status (5/5)** - NEW: Voltage, level, charging state
- **Sensor pages**: (unchanged)
  1. Status (1/3) - Uptime, last TX
  2. TX Statistics (2/3) - Success rate
  3. Battery Status (3/3) - Battery info

### Enhanced - WiFi Status Display

- **Dynamic WiFi indicator**: Base station now correctly shows WiFi connection status
- **Fixed detection logic**: Removed hardcoded WIFI_SSID check, now uses WiFi.status()
- **Real-time updates**: WiFi icon reflects actual connection state

### Technical Improvements

#### Libraries Added

- **ESP Async WebServer 1.2.3**: Non-blocking web server for captive portal
- **DNSServer 2.0.0**: DNS redirect for captive portal detection
- **QRCode 0.0.1**: QR code generation for OLED display
- **Preferences 2.0.0**: ESP32 NVS access for persistent storage

#### Code Architecture

- **New modules**:
  - `config_storage.h/.cpp`: NVS management and device configuration
  - `wifi_portal.h/.cpp`: Complete captive portal implementation
- **Enhanced modules**:
  - `display_control.h/.cpp`: Added button handling, QR display, ping functions
  - `statistics.h/.cpp`: Added sensor timeout tracking functions
  - `main.cpp`: Refactored for dynamic configuration and mode detection

#### Memory Usage

- **Base Station**: 849,253 bytes (25.4% flash)
- **Sensor**: 852,457 bytes (25.5% flash)
- **RAM**: ~47KB used (14.4% of 320KB)

### Fixed

#### Critical Fixes

- **Watchdog timer crash**: Fixed ESP32 task watchdog timeout during WiFi network scanning
  - Root cause: Blocking WiFi.scanNetworks() prevented async_tcp task from yielding
  - Solution: Added 300ms per-channel timeout parameter to WiFi.scanNetworks()
  - Impact: Captive portal now stable when selecting base station mode

#### Display Fixes

- **QR code screen clearing**: Fixed persistent "First Boot" text visible after rotation

  - Solution: Clear buffer before rotation, fill black background, then draw QR
  - Result: Clean QR code display with proper text positioning

- **Signal graph overflow**: Fixed graph lines and text running off screen edges

  - Y boundaries: Map values to (1, height-1) instead of (0, height)
  - Layout: Adjusted graph position and height for proper fit
  - Text positioning: Moved labels to Y=52 to fit within 64-pixel display

- **QR code overlap**: Text "Scan to connect" no longer overlaps QR code
  - Solution: Rotated display 90° to portrait mode (64x128)
  - Result: QR code fits comfortably with text below

### Changed

#### Configuration Changes

- **Dynamic sensor intervals**: Removed hardcoded SENSOR_INTERVAL, now user-configurable
- **Dynamic sensor IDs**: Removed hardcoded SENSOR_ID, now configured via portal
- **WiFi credentials**: Removed hardcoded WIFI_SSID/WIFI_PASSWORD, now portal-configured
- **IP address scheme**: Changed AP IP from 192.168.4.1 to 10.8.4.1

#### Behavioral Changes

- **First boot behavior**: Devices now start in AP mode instead of attempting to operate
- **Factory reset**: Reduced hold time from 10 seconds to 5 seconds
- **Button responsiveness**: Improved click detection with proper debouncing
- **Display rotation**: QR code page now uses portrait orientation

### Documentation

#### Updated Files

- **README.md**: Updated to reflect new WiFi configuration system
- **FEATURES.md**: Marked WiFi configuration features as completed
- **CHANGELOG.md**: Created comprehensive change log (this file)

#### Code Comments

- Added detailed comments for WiFi scan timeout fix
- Documented button state machine logic
- Explained QR code display rotation reasoning

---

## [1.0.0] - 2025-12-09

### Initial Release

#### Core Features

- LoRa communication between base station and sensors
- Multi-page OLED display with automatic cycling
- Temperature monitoring via thermistor
- Battery voltage and percentage monitoring
- Real-time statistics tracking
- LED status indicators
- User button for display wake/page cycling

#### Hardware Support

- Heltec WiFi LoRa 32 V3 (ESP32-S3)
- SX1262 LoRa radio (868MHz EU868)
- SSD1306 128x64 OLED display
- WS2812 NeoPixel LED
- 10kΩ NTC thermistor

#### Base Station Features

- 4-page cycling display
- Tracks up to 10 sensors
- RSSI history graph
- RX statistics
- Sensor list with battery levels

#### Sensor Node Features

- 3-page cycling display
- Configurable transmission intervals
- TX statistics tracking
- Battery status display
- Automatic LED feedback

---

## Version History

- **2.0.0** (2025-12-12): WiFi captive portal, dynamic configuration, advanced button controls
- **1.0.0** (2025-12-09): Initial release with basic LoRa communication

---

_For detailed feature roadmap, see [FEATURES.md](FEATURES.md)_
