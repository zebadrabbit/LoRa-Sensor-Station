# Changelog

All notable changes to the LoRa Sensor Station project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.16.0] - 2025-12-18

### Added

#### Sensor Startup Announcement Protocol

- **Startup Announcement**: Sensors announce themselves to base station on boot with `CMD_SENSOR_ANNOUNCE`
- **Automatic Time Sync**: Base station responds with `CMD_TIME_SYNC` via reliable command queue
- **Sensor ID in Payload**: Announcement includes sensor ID in packet payload for proper identification
- **3-Hour Periodic Sync**: Sensors automatically request time sync every 3 hours to maintain accuracy
- **Minimal Traffic**: Single announcement packet per 3 hours keeps overhead minimal

#### Display Improvements

- **Fixed Page Indicators**: All base station pages show correct 1/8 through 8/8 (was 1/7-6/7)
- **Fixed Time Display Threshold**: Changed from `> 1000` to `> 1000000000` for valid Unix timestamps (after year 2001)
- **Sensor Summary Page**: Replaced cramped sensor list on page 3/8 with useful summary showing:
  - Active sensor count
  - Total sensors seen
  - Oldest sensor last-seen time
- **WiFi Monitoring**: Page 7/8 now includes both uptime and RSSI signal strength
- **Dynamic Uptime Format**: Shows as 23s, 45m, 3h, or 5d based on duration
- **RSSI Display**: Shows WiFi signal strength in dBm for connection quality monitoring

### Changed

#### Sensor ID Filtering

- **Target Validation**: Sensors now check `targetSensorId` field before processing commands
- **Ignore Non-Matching**: Commands not intended for specific sensor are ignored
- **Debug Logging**: Added logging to show when commands are filtered

#### Time Synchronization Architecture

- **Leveraged Existing Infrastructure**: Uses proven `CMD_TIME_SYNC` via `queueCommand()` instead of custom welcome packet
- **Reliable Delivery**: Benefits from existing retry logic, checksums, and radio state management
- **Simplified Code**: Removed custom `CMD_BASE_WELCOME` packet handling in favor of unified approach

### Fixed

- **Base Station Time Broadcast**: Now only sends time sync to active sensors (not all 256 possible IDs)
- **Startup Time Sync**: Base station waits for NTP sync then broadcasts to active sensors only
- **Radio State Management**: Proper use of command queue prevents radio state conflicts from direct packet sends

## [2.15.0] - 2025-12-14

### Added - Phase 2: I2C Sensor Support

#### Multi-Sensor Support

- **I2C sensor integration**: Support for BME680, BH1750, and INA219 sensors
- **Auto-detection**: Automatic I2C bus scanning and sensor identification on boot
- **Multi-value packets**: New `MultiSensorPacket` format supporting up to 16 sensor values
- **Value types**: Temperature, humidity, pressure, gas resistance, light, voltage, current, power, and more
- **Sensor manager**: `SensorManager` class for unified sensor lifecycle management
- **Dynamic packet sizing**: Variable-length packets based on active sensor count

#### BME680 Environmental Sensor

- **4-in-1 sensor**: Temperature, humidity, pressure, and gas resistance
- **I2C addresses**: Support for 0x76 and 0x77
- **Gas measurement**: Indoor air quality indicator (kΩ)
- **Automatic calibration**: Bosch BME680 library integration
- **Value types**: `VALUE_TEMPERATURE`, `VALUE_HUMIDITY`, `VALUE_PRESSURE`, `VALUE_GAS_RESISTANCE`

#### BH1750 Light Sensor

- **Ambient light measurement**: 1-65535 lux range
- **I2C addresses**: Support for 0x23 (default) and 0x5C (alternate)
- **High-resolution mode**: CONTINUOUS_HIGH_RES_MODE for accurate readings
- **Heltec library**: Uses built-in BH1750 library (conflicts with external library resolved)
- **Value type**: `VALUE_LIGHT`

#### INA219 Power Monitor

- **3-in-1 sensor**: Voltage, current, and calculated power
- **I2C address**: 0x40 (default)
- **Measurement ranges**: 0-26V, 0-3.2A, 0-83W
- **Calibration**: Pre-configured for 32V/2A range
- **Value types**: `VALUE_VOLTAGE`, `VALUE_CURRENT`, `VALUE_POWER`

#### Dashboard Enhancements

- **New charts**: Humidity, pressure, gas resistance, light, voltage, current, power
- **Real-time display**: All sensor types shown in live dashboard
- **Chart initialization**: Auto-initialize charts for all supported sensor types
- **Historical data**: Chart.js integration for time-series visualization
- **Unit display**: Proper units (%, hPa, kΩ, lux, V, mA, mW) for each sensor type

#### MQTT Integration

- **Multi-sensor publishing**: New `publishMultiSensorData()` function
- **Individual topics**: Each sensor value published to dedicated topic
  - `lora/sensor/{id}/temperature`
  - `lora/sensor/{id}/humidity`
  - `lora/sensor/{id}/pressure`
  - `lora/sensor/{id}/gas_resistance`
  - `lora/sensor/{id}/light`
  - `lora/sensor/{id}/voltage`
  - `lora/sensor/{id}/current`
  - `lora/sensor/{id}/power`
- **Combined JSON state**: All readings in single JSON message at `lora/sensor/{id}/state`
- **Home Assistant auto-discovery**: `publishHomeAssistantMultiSensorDiscovery()`
- **Device classes**: Proper HA device classes (temperature, humidity, pressure, illuminance, voltage, current, power)
- **Automatic units**: Correct units and icons in Home Assistant

#### Sensor Data Storage

- **PhysicalSensor tracking**: Individual sensor readings stored in `PhysicalSensor` structures
- **Per-value history**: 100-point ring buffers for each sensor type
- **updateSensorReading()**: Store readings by clientId, sensorIndex, type, and value
- **getSensor()**: Retrieve individual sensor data
- **Historical API**: Foundation for future history endpoints for all sensor types

#### Firmware Changes

- **Sensor firmware size**: 1,133,677 bytes (33.9% flash), 105,096 bytes (32.1% RAM)
- **Library integration**: Heltec_BH1750 for light sensor (linker conflicts resolved)
- **Multi-sensor packet handling**: Both direct and mesh-routed packets supported
- **Sensor value printing**: Debug output for all received sensor values with type names

#### Documentation

- **SENSOR_WIRING.md**: Comprehensive 631-line wiring guide
  - Pin reference (power, I2C, ADC, reserved pins)
  - Thermistor voltage divider circuit (10kΩ resistor + thermistor)
  - I2C sensor specifications (BME680, BH1750, INA219)
  - JST connector pinout: Pin 1=GND, 2=3.3V, 3=SDA(41), 4=SCL(42)
  - Troubleshooting steps and safety warnings
  - Example multi-sensor setups and calibration info

### Fixed

- **NVS error spam**: Added `prefs.isKey()` check in `config_storage.cpp` before reading `sensor_zone`
  - Prevents "nvs_get_str len fail: sensor_zone NOT_FOUND" errors on sensor nodes
  - Zone field only exists on base station, not sensor nodes
- **Duplicate boot output**: Removed redundant encryption/whitelist status from `main.cpp`
  - Kept emoji version (🔒) from `security.cpp`
  - Cleaner boot sequence with single security status display
- **BH1750 linker conflict**: Removed external BH1750 library dependency
  - Using Heltec's built-in BH1750 implementation instead
  - Resolved "multiple definition" linker errors

### Changed

- **Packet format**: Transitioned from legacy `SensorData` to `MultiSensorPacket`
- **MQTT publishing**: Base station now uses `publishMultiSensorData()` for new packets
- **Home Assistant discovery**: Switched to `publishHomeAssistantMultiSensorDiscovery()` for multi-sensor devices
- **Sensor storage**: Mesh-routed packets now store individual sensor readings via `updateSensorReading()`
- **Dashboard layout**: Expanded grid to accommodate 10 chart types (temp, humid, pressure, gas, light, voltage, current, power, battery, RSSI)

### Known Limitations

- **Historical data**: Current history API returns battery/RSSI only
  - Individual sensor readings stored but not yet exposed via `/api/history`
  - Future enhancement: Add multi-sensor historical data endpoints
- **Hardware testing**: Phase 2 features implemented but not tested with physical sensors
  - 10kΩ resistors for thermistors ordered (arriving next week)
  - BME680, BH1750, INA219 sensors ordered (arriving next week)
- **Chart updates**: Dashboard charts show historical battery/RSSI data
  - Sensor value charts will populate once historical API enhanced

### Critical TODO

- **⚠️ LoRa Settings Sync Protocol**: Changing base station LoRa settings currently breaks sensor communication
  - Need: `SET_LORA_PARAMS` command to propagate settings to all sensors
  - Required: Coordinated reboot protocol (broadcast → ACK → clients reboot → base reboots)
  - Impact: Without this, changing frequency/SF/BW orphans all sensors
  - Documented in FEATURES.md as CRITICAL priority

## [2.14.0] - 2025-12-14

### Added - Multi-Sensor Support: Zones, Priority, and Health Scoring

#### Sensor Zones and Organization

- **Zone field**: 16-character zone/area field for sensor grouping (e.g., "Outdoor", "Garage", "Bedroom")
- **Zone configuration**: Added zone input field to sensor captive portal
- **Zone API endpoints**: GET/POST `/api/sensors/{id}/zone` for reading/updating zones
- **Zone filtering**: Dropdown filter on dashboard to show only sensors in specific zone
- **Zone badges**: Visual zone indicators on dashboard sensor cards
- **NVS persistence**: Zones stored in both sensor config and metadata namespaces

#### Sensor Priority Levels

- **Priority enum**: LOW (0), MEDIUM (1), HIGH (2) priority levels
- **Priority configuration**: Priority selector in sensor captive portal (Low/Medium/High)
- **Priority API endpoints**: GET/POST `/api/sensors/{id}/priority` for priority management
- **Priority badges**: Color-coded badges on dashboard (gray=Low, blue=Medium, red=High)
- **Priority-based alerts**: Alert rate limiting adjusted by priority:
  - HIGH: 50% of standard rate (more frequent alerts)
  - MEDIUM: 100% of standard rate (default 15 minutes)
  - LOW: 400% of standard rate (1 hour cooldown)
- **NVS persistence**: Priority stored in both sensor config and metadata

#### Advanced Health Scoring System

- **Health score structure**: 8-field SensorHealthScore with comprehensive metrics:
  - `communicationReliability` (0.0-1.0): Packet success rate with sample penalty
  - `readingQuality` (0.0-1.0): Variance-based sensor quality metric
  - `batteryHealth` (0.0-1.0): Voltage-based with 1%/month degradation
  - `overallHealth` (0.0-1.0): Weighted average (50% comms, 20% quality, 30% battery)
  - `uptimeSeconds`: Time since first packet received
  - `lastSeenTimestamp`: Timestamp of last contact (millis)
  - `totalPackets`: Count of all received packets
  - `failedPackets`: Count of lost/failed packets
- **Health calculation algorithms**:
  - Communication reliability: `(successPackets / totalPackets) * min(totalPackets, 10)/10`
  - Battery health: `((voltage - 3.0) / 1.2) * (1 - uptimeMonths * 0.01)`
  - Reading quality: `1.0 / (1.0 + variance)` (variance-based)
- **Health tracking NVS keys** (per sensor):
  - `s{id}_htot`: Total packets received
  - `s{id}_hfail`: Failed packet count
  - `s{id}_hupt`: Uptime in seconds
  - `s{id}_hlast`: Last seen timestamp
  - `s{id}_hfirst`: First seen timestamp
  - `s{id}_hbatt`: Battery voltage reading
  - `s{id}_htemp`: Temperature reading
- **Health API endpoint**: GET `/api/sensors/{id}/health` returns complete health metrics
- **Automatic updates**: `updateHealthScore()` called on every packet reception in `updateSensorInfo()`
- **Dashboard health indicators**: Color-coded badges (green >0.8, yellow 0.5-0.8, red <0.5)

#### Dashboard Enhancements

- **Updated API**: `/api/sensors` now includes zone, priority, priorityLevel, and health object
- **Zone filter**: Dropdown automatically populated with unique zones from active sensors
- **Enhanced sensor cards**: Display location, ID, zone badge, priority badge, health badge
- **Health tooltips**: Hover to see detailed health percentage
- **Battery format**: Maintains existing "On AC/15%/Charging" format
- **Auto-refresh**: Zone filter preserved across auto-refresh cycles

#### LoRa Settings Configuration Page

- **New page**: `/lora-settings` route with comprehensive radio configuration
- **Dashboard card**: "📻 LoRa Settings" card added to main dashboard
- **Regional frequency bands**: US915, EU868, AU915, AS923, CN470, IN865
- **Configurable parameters**:
  - Region selection (auto-updates center frequency)
  - Center frequency (Hz)
  - Spreading factor (SF7-SF12)
  - Bandwidth (125/250/500 kHz)
  - TX power (2-20 dBm)
  - Coding rate (4/5, 4/6, 4/7, 4/8)
  - Preamble length (6-65535)
- **API endpoints**: GET/POST `/api/lora/config` for configuration management
- **Current values display**: Shows active LoRa configuration at top of page
- **Helpful descriptions**: Detailed explanations for each parameter
- **Auto-fill**: Region selection automatically updates frequency field
- **Compliance warnings**: Reminders about RF regulations

#### Memory and Performance

- **Memory impact**: ~49 bytes per sensor (~1.5KB for 32 sensors)
- **NVS usage**: Health data in separate "sensor-health" namespace
- **Efficient storage**: Short NVS keys to conserve flash space
- **No external dependencies**: Uses existing NVS and ESP32 APIs

### Changed

- **SensorConfig structure**: Added `zone[16]` and `priority` fields
- **SensorMetadata structure**: Added `zone[16]`, `priority`, and `health` fields
- **Alert rate limiting**: Now priority-aware with dynamic cooldown periods
- **Statistics tracking**: Health scores updated on every packet reception

### Technical Details

- **Backward compatible**: Existing sensors work without zone/priority configuration
- **Default values**: Zone defaults to empty string, priority defaults to MEDIUM
- **Health initialization**: Health scores start tracking on first packet
- **Degradation model**: 1% per month battery degradation (realistic for Li-ion)
- **Sample penalty**: Low packet counts (<10) penalized to avoid false confidence
- **NVS namespaces**: "sensor-meta" for config, "sensor-health" for tracking

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
- **Display format**: "ID: 1 Net: 12345 [E]" for encrypted sensors
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
- **Double click** (screen on): Send immediate ping (sensors only)
- **Triple click** (screen on): Reboot device
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
