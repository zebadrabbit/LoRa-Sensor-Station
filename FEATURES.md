# LoRa Sensor Station - Feature Roadmap

## Version History

### v2.16.0 (December 15, 2025) - Current Version

- ✅ **LoRa Settings Coordinated Reboot** 🎯 COMPLETED
- ✅ ACK tracking system for multi-sensor coordination
- ✅ Base station waits for all sensors to ACK before rebooting
- ✅ Real-time progress monitoring: "Waiting for sensor ACKs... (2/4)"
- ✅ 20-second timeout with partial ACK handling
- ✅ Sensors auto-reboot 5s after ACK
- ✅ Base station reboots 8s after all ACKs (or timeout)
- ✅ API endpoint `/api/lora/reboot-status` for status polling
- ✅ Frontend modal shows live ACK progress per sensor
- ✅ **UNBLOCKS**: /lora-settings page now fully functional
- ✅ **Flash**: 43.7% (1,460,953 bytes), RAM: 33.2% (108,792 bytes)

### v2.15.3 (December 15, 2025)

- ✅ **UI Standardization Complete**
- ✅ Semantic HTML across all config pages (header/article tags)
- ✅ pico-custom.css applied consistently
- ✅ Removed duplicate Config button from dashboard
- ✅ **Enhanced UX**
- ✅ 5px colored tab border on client cards
- ✅ Location sync bug fixed (base station updates immediately)
- ✅ Loading states on runtime-config buttons (spinner: "⏳ Waiting for ACK...")
- ✅ Button disabling during command execution
- ✅ Auto-refresh after successful commands (2-3s delay)

### v2.15.2 (December 14, 2025)

- ✅ **Client Type Selection**: Initial setup configuration
- ✅ Three client types: Standard (AC+battery), Rugged (solar+battery), Deep Sleep (ultra low power)
- ✅ Client type persisted in NVS configuration
- ✅ Captive portal dropdown with detailed descriptions
- ✅ Prepares foundation for future deep sleep and power management features
- ✅ **Code Cleanup**: Removed redundant /sensors page
- ✅ Consolidated sensor configuration into /runtime-config page
- ✅ Removed 279 lines of duplicate code (generateSensorNamesPage, generateSensorNamesJSON, handleSensorNameUpdate)
- ✅ Streamlined web interface with single configuration page
- ✅ **Bug Fixes**:
- ✅ Removed duplicate chart card sections from dashboard (46 lines)
- ✅ Fixed dashboard showing empty cards for environmental sensors
- ✅ **Hardware Documentation**:
- ✅ Added piezo buzzer wiring for both sensor and base station
- ✅ GPIO 5 (sensor) and GPIO 6 (base station)
- ✅ Active vs passive buzzer guidance with code examples

### v2.15.1 (December 14, 2025)

- ✅ **Pico-Inspired CSS Framework**
- ✅ Created pico-custom.css (6.44KB) - lightweight CSS framework
- ✅ Semantic HTML conversion (article/header tags)
- ✅ Consistent styling across all web pages
- ✅ **Dashboard Chart Fixes**
- ✅ Fixed chart visibility logic (article selector)
- ✅ Charts hide when no data available
- ✅ Battery/RSSI updates working correctly
- ✅ Status indicator with inline CSS colors
- ✅ Client cards with hover effects and grid layout

### v2.15.0 (December 14, 2025)

- ✅ **I2C Sensor Support (Phase 2)**: BME680, BH1750, INA219
- ✅ BME680 environmental sensor (temperature, humidity, pressure, gas resistance)
- ✅ BH1750 light intensity sensor (ambient light in lux)
- ✅ INA219 power monitor (voltage, current, power)
- ✅ Auto-detection on I2C bus (addresses 0x23, 0x40, 0x5C, 0x76, 0x77)
- ✅ Multi-sensor MQTT publishing with individual topics per sensor type
- ✅ Home Assistant multi-sensor auto-discovery
- ✅ Sensor wiring documentation (SENSOR_WIRING.md)
- ✅ **Display Improvements**: Welcome pages, longer page cycle
- ✅ Page 0 welcome screens ("Hello! I am [name]")
- ✅ Adaptive font sizing for long names (>14 chars)
- ✅ Page cycle increased from 5s to 10s
- ✅ Button press resets page timer (already implemented)
- ✅ 7 pages base station, 5 pages sensor
- ✅ **Button Action Swap**: 2 clicks=ping, 3 clicks=reboot
- ✅ Double click sends immediate ping (was reboot)
- ✅ Triple click reboots device (was ping)
- ✅ More intuitive UX (common action easier, destructive action safer)
- ✅ **LoRa Settings Sync**: Coordinated reboot protocol
- ✅ Web UI for changing LoRa parameters (frequency, SF, bandwidth, TX power, etc.)
- ✅ Coordinated reboot: sensors reboot in 5s, base station in 10s
- ✅ 5-step modal with timing display and reboot sequence
- ✅ Parameter persistence via NVS (lora_params namespace)
- ✅ Boot screen displays actual frequency and sensor ID
- ✅ Bandwidth conversion fix (kHz to Hz) and auto-correction
- ✅ Default bandwidth changed to 125000 Hz
- ✅ Parameters always loaded from NVS if stored (not just when pending flag set)
- ✅ API endpoint reads actual stored parameters (not hardcoded values)
- ✅ **Dashboard UI Improvements**
- ✅ Historical data charts hide when no data available (temp, humidity, pressure, gas, light, voltage, current, power)
- ✅ Only battery and RSSI charts shown (these are tracked in history)
- ✅ Status indicator inline with sensor name (improved layout)
- ✅ **Remote Configuration**
- ✅ Forced 10s interval after command reception (60 second duration)
- ✅ Runtime configuration web page (/runtime-config)
- ✅ Remote interval adjustment via web UI
- ✅ Remote location/name change via web UI
- ✅ Remote sensor restart command
- ✅ Quick response even from 30-minute interval sensors

### v2.14.0 (December 14, 2025)

- ✅ Multi-Sensor Support: Zones, Priority, Health Scoring
- ✅ Sensor zone/area grouping (16-character field, NVS-persisted)
- ✅ Sensor priority levels (LOW/MEDIUM/HIGH)
- ✅ Advanced health scoring system (communication, battery, quality)
- ✅ Health tracking: uptime, packet success rate, degradation
- ✅ Zone filter on dashboard (group sensors by location)
- ✅ Priority badges on dashboard (visual indicators)
- ✅ Health indicators (color-coded: green/yellow/red)
- ✅ Priority-based alert rate limiting (HIGH = 50%, LOW = 400%)
- ✅ Captive portal zone/priority configuration
- ✅ 5 new API endpoints (zone, priority, health)
- ✅ LoRa settings configuration page (/lora-settings)
- ✅ Regional frequency band selection (US915, EU868, AU915, etc.)
- ✅ Configurable spreading factor, bandwidth, TX power
- ✅ Automatic health score updates on packet reception

### v2.13.0 (December 14, 2025)

- ✅ Network Pairing Phase 2: Encryption & Whitelisting
- ✅ AES-128-CBC encryption using ESP32 mbedTLS (hardware-accelerated)
- ✅ Random IV generation per packet (16 bytes)
- ✅ HMAC-based message authentication (8-byte)
- ✅ Replay attack prevention (sequence numbers + 100-packet sliding window)
- ✅ Device whitelist (up to 32 devices, NVS-persisted)
- ✅ Security management web UI (/security)
- ✅ Encryption key management (view, copy, generate, set custom)
- ✅ 9 security API endpoints (config, whitelist, key management)
- ✅ Sensor configuration with encryption key field
- ✅ Display security indicator ([E] when encryption enabled)
- ✅ Backward compatible (encrypted/unencrypted devices coexist)
- ✅ Zero external dependencies (uses ESP32 framework mbedTLS)

### v2.12.2 (December 14, 2025)

- ✅ Sensor display shows ID + Network ID (first status page)
- ✅ Dashboard width standardized to 800px (matches all config pages)
- ✅ Alerts page moved to LittleFS filesystem (improved performance)
- ✅ Client configuration page title/labels corrected
- ✅ /sensors page JavaScript syntax errors fixed
- ✅ Network Pairing Phase 1 fully tested and validated
- ✅ Consistent page widths across all web interfaces
- ✅ Memory optimization (large HTML moved to filesystem)

### v2.12.1 (December 13, 2025)

- ✅ Base station captive portal WiFi scan removed (watchdog timeout fix)
- ✅ Manual SSID entry for base station configuration
- ✅ Improved success page with countdown timer
- ✅ "Setup Complete" message with device rebooting status
- ✅ Dashboard "Forget" button for removing inactive clients
- ✅ API endpoint: DELETE /api/clients/{id} for client removal
- ✅ Client location display fixed (shows configured name)
- ✅ Sensor config help text updated ("client location" vs "sensor location")
- ✅ Confirmation dialog before forgetting clients

### v2.12.0 (December 13, 2025)

- ✅ Network Pairing & Security (Phase 1)
- ✅ Network ID (1-65535) user-configurable field
- ✅ LoRa sync word calculation: 0x12 + (networkId % 244)
- ✅ Hardware filtering via Radio.SetSyncWord()
- ✅ Software validation: networkId in packet headers
- ✅ Captive portal Network ID field (sensor + base station)
- ✅ NVS storage for network configuration
- ✅ Code quality tools: clang-format, cppcheck, clang-tidy

### v2.11.0 (December 13, 2025)

- ✅ LittleFS filesystem implementation (fixed memory exhaustion)
- ✅ Separate HTML/CSS/JS files (~7.5KB compressed)
- ✅ Streaming from flash (zero RAM usage for web files)
- ✅ Client/Sensor architecture separation
- ✅ ClientInfo (device telemetry) vs PhysicalSensor (readings)
- ✅ Dual history buffers (ClientHistory + SensorHistory)
- ✅ API refactoring (client telemetry only, no mixed data)
- ✅ Dashboard improvements (client-focused display)
- ✅ Battery format: "On AC/15%/Charging"
- ✅ Auto-hide/show charts based on data availability
- ✅ Fixed Active Clients counter bug
- ✅ Legacy compatibility layer maintained
- ✅ System ready for multi-sensor expansion

### v2.10.0 (December 13, 2025)

- ✅ Remote Configuration via LoRa (SET_INTERVAL command)
- ✅ Command retry system (3 attempts, 10s timeout)
- ✅ Piggyback ACK in telemetry headers (2 bytes)
- ✅ Immediate ACK transmission after command processing
- ✅ Sensor continuous RX mode (always listening)
- ✅ HTTP API endpoint for remote commands
- ✅ Command status tracking and reporting
- ✅ Base station dashboard fix (proper page served at /)
- ✅ Network pairing feature added to roadmap

### v3.0.0 - Mesh Network Support ✅ IN PROGRESS (2025-01-13)

**Status:** Core implementation complete, testing needed

- [x] AODV-like reactive routing protocol
- [x] Mesh packet structures (5 types: DATA, RREQ, RREP, RERR, BEACON)
- [x] Route discovery via flooding
- [x] Routing table management (32 entries, 10-minute timeout)
- [x] Neighbor discovery (30-second beacons)
- [x] Packet forwarding with TTL (max 5 hops)
- [x] Duplicate detection (sequence numbers + cache)
- [x] Network health monitoring
- [x] Integration with existing LoRa communication
- [x] Configuration storage (enable/disable mesh)
- [ ] 3-node topology testing
- [ ] Web dashboard mesh status page

**Configuration Options:**

- Sensor nodes: meshEnabled (default: **false**), meshForwarding (default: true)
- Base station: meshEnabled (default: **false**)
- **Mesh is disabled by default** for backward compatibility
- Stored in NVS, persists across reboots
- Applied on boot, checked in main loop
- Only processes mesh packets when meshEnabled=true

**Architecture:**

- Transparent to application layer (sensors just send to base station)
- Self-organizing topology via neighbor beacons
- Self-healing routes (rediscovery on timeout)
- Backward compatible with non-mesh nodes
- Link quality tracking via RSSI

**Flash Usage:**

- Sensor: 33.3% (1,114,257 bytes)
- Base Station: 42.8% (1,429,617 bytes)

---

## 🔴 High Priority - Core Functionality

### Remote Configuration

- [x] **LoRa Remote Configuration** ✅ COMPLETED v2.10.0

  - Remote interval adjustment via LoRa ✅
  - Command queueing with sequence numbers ✅
  - Piggyback ACK in telemetry packets ✅
  - 3-retry system with 10-second timeout ✅
  - Immediate ACK transmission after command ✅
  - Sensor continuous RX mode (always listening) ✅
  - HTTP API for command sending ✅
  - Command status tracking and reporting ✅
  - No WiFi required on sensors ✅

  **API Endpoint:**

  ```
  POST http://[base-ip]/api/remote-config/interval
  Body: {"id": 1, "interval": 30}
  ```

  **Command Flow:**

  1. HTTP POST to base station API
  2. Command queued and sent immediately (sensor always listening)
  3. Sensor receives, processes, saves ACK status
  4. Sensor sends immediate telemetry with ACK
  5. Base receives ACK → command cleared (or retries up to 3x)

  **Still TODO for Full Remote Config:**

  - [x] **Remote LoRa Settings Sync** ✅ COMPLETED v2.16.0
    - [x] SET_LORA_PARAMS command (frequency, SF, BW, TX power) ✅
    - [x] Coordinated reboot protocol: ✅
      1. Base station sends command to all registered clients ✅
      2. Wait for all clients to ACK receipt ✅
      3. Clients reboot to apply new settings (5s after ACK) ✅
      4. Base station reboots to apply new settings (8s after all ACKs) ✅
      5. All nodes come back online with matching parameters ✅
    - [x] Timeout handling (20-second timeout, proceeds with partial ACKs) ✅
    - [x] Real-time progress monitoring via `/api/lora/reboot-status` ✅
    - [x] Frontend modal with live ACK tracking per sensor ✅
    - [ ] Rollback mechanism if migration fails (not implemented - manual recovery)
  - [x] Additional command types: location ✅, interval ✅, restart ✅ (v2.15.3)
  - [x] Web UI for remote configuration ✅ (/runtime-config page, v2.15.0)
  - [ ] Additional command types: thresholds, mesh config (future)
  - [ ] Command history logging
  - [ ] Batch commands (send same command to multiple sensors simultaneously)

### WiFi Configuration & Management

- [x] **Captive Portal for WiFi Setup** ✅ COMPLETED v2.0.0

  **Sensor Client Path:** ✅ IMPLEMENTED

  1. Device boots in AP mode (SSID: "LoRa-Sensor-XXXX") ✅
  2. User connects phone/laptop to AP (password: "configure") ✅
  3. Captive portal automatically opens (DNS redirect) ✅
  4. User enters configuration: ✅
     - Unique Sensor ID (1-255) ✅
     - Sensor Location/Name (text field) ✅
     - Zone/Area (optional grouping field) ✅
     - Sensor Priority (Low/Medium/High) ✅
     - Client Type (Standard/Rugged/DeepSleep) ✅ NEW
     - Network ID (pairing) ✅
     - Transmission Interval (15s/30s/60s/5min) ✅
     - Encryption Key (optional) ✅
  5. Save → Device stores config in NVS ✅
  6. Device reboots into sensor mode (LoRa only, no WiFi) ✅
  7. Display shows sensor status with configured settings ✅

  **Client Types:** ✅ NEW

  - **Standard (CLIENT_STANDARD)**: AC power with battery backup, normal operation
  - **Rugged (CLIENT_RUGGED)**: Solar-powered outdoor sensor with weather-resistant battery
  - **Deep Sleep (CLIENT_DEEPSLEEP)**: Battery-only with deep sleep mode for extended battery life

  **Base Station Path:** ✅ IMPLEMENTED

  1. Device boots in AP mode (SSID: "LoRa-Base-XXXX") ✅
  2. User connects phone/laptop to AP (password: "configure") ✅
  3. Captive portal automatically opens (DNS redirect) ✅
  4. User configures base station: ✅
     - WiFi network scan (300ms timeout per channel) ✅
     - Select network from list ✅
     - Enter WiFi password (open network support) ✅
     - Test Connection before saving ✅
  5. Connection validation: ✅
     - Success: Save to NVS, reboot to base station mode ✅
     - Failure: Show error, allow retry ✅
  6. Base station runs WiFi + LoRa, displays IP address ✅

  **Common Features:** ✅ IMPLEMENTED

  - Responsive HTML/CSS (mobile-friendly) ✅
  - DNS redirect for captive portal detection ✅
  - QR code display (portrait mode, 90° rotation) ✅
  - Factory reset (5s button hold) clears config ✅
  - Multi-click button detection (1=wake/next, 2=reboot, 3=ping, 5s=reset) ✅
  - First-boot detection with NVS storage ✅
  - Display shows QR code and AP credentials ✅

- [x] **Web Dashboard** ✅ COMPLETED v2.1.0

  - Real-time sensor data dashboard ✅
  - Auto-refresh (5-second intervals) ✅
  - Responsive mobile-friendly design ✅
  - API endpoints (JSON sensor data) ✅
  - Export data as CSV/JSON ✅
  - Color-coded status indicators ✅
  - System statistics display ✅

  **Still TODO for Full Dashboard:**

  - [x] Historical data graphs (hourly/daily/weekly) ✅ COMPLETED v2.7.0
  - [x] WebSocket for live updates (currently polling) ✅ COMPLETED v2.6.0
  - [x] Alert configuration interface ✅ COMPLETED v2.3.0
  - [x] Configuration page for runtime settings ✅ COMPLETED v2.15.0- [x] Pico-inspired CSS framework ✅ COMPLETED v2.15.1

- [x] Dashboard chart visibility fixes ✅ COMPLETED v2.15.1 - [ ] OTA firmware update interface

  - [ ] Network diagnostics page

  **Note:** Base station now serves live dashboard at http://[IP]/ showing real-time sensor data with auto-refresh.

### Alerts & Notifications

- [x] **Microsoft Teams Integration** ✅ COMPLETED v2.3.0

  - Webhook configuration via web interface ✅
  - Alert on temperature thresholds ✅
  - Alert on battery low (<20%) ✅
  - Alert on sensor offline (timeout) ✅
  - Alert on communication failures ✅
  - Customizable alert templates ✅
  - Rate limiting to prevent spam ✅

- [x] **Email Notifications** ✅ COMPLETED v2.5.0

  - SMTP configuration (Gmail, Outlook, etc.) ✅
  - HTML formatted email alerts ✅
  - Multiple recipient support (comma-separated) ✅
  - Test email functionality ✅
  - TLS/STARTTLS support ✅
  - Dual-channel alerts (Teams + Email) ✅

  **Still TODO for Email:**

  - [ ] Attachment support (logs, graphs)
  - [ ] Email digest (daily/weekly summaries)

- [ ] **SMS Gateway Integration**
  - Twilio API integration
  - Critical alerts only (battery critical, sensor lost)
  - Phone number whitelist
  - SMS rate limiting (cost control)
  - Delivery confirmation

### Data Logging & Storage

- [x] **MQTT Publishing** ✅ COMPLETED v2.8.0

  - MQTT broker connection with authentication ✅
  - Individual topic publishing per metric ✅
  - Combined JSON state topic ✅
  - Home Assistant auto-discovery ✅
  - QoS support (0, 1, 2) ✅
  - Auto-reconnect with exponential backoff ✅
  - Web configuration interface ✅
  - Connection statistics and testing ✅

- [ ] **Cloud Data Storage**
  - InfluxDB integration
  - ThingSpeak support
  - Google Sheets integration
  - Custom REST API endpoints
  - Data retention policies

## 🟡 Medium Priority - Enhanced Features

### Power Management

- [ ] **Deep Sleep Mode**

  - Configurable sleep intervals for sensors
  - Wake on button press
  - Wake on timer for transmission
  - Battery life estimation
  - Low-power LoRa modes
  - Dynamic power adjustment based on battery level

- [ ] **Solar Panel Support**
  - Charging status monitoring
  - Solar panel voltage/current monitoring
  - Charge controller integration
  - Battery health tracking
  - Optimal charging algorithms

### Advanced Sensor Features

- [x] **Sensor Health Monitoring** ✅ COMPLETED v2.0.0

  - Automatic timeout detection (15-minute threshold) ✅
  - Periodic health checks (every 30 seconds) ✅
  - Serial logging for offline sensors ✅
  - isSensorTimedOut() API ready for alerts ✅
  - Configurable timeout based on transmission interval ✅

- [x] **Multi-Sensor Support**

  - Support for 20+ sensors (currently 10) ✅
  - Sensor grouping/zones ✅ COMPLETED v2.14.0
  - Sensor priority levels ✅ COMPLETED v2.14.0
  - Auto-discovery of new sensors ✅ (basic implementation)
  - Sensor naming/labeling via portal ✅ (location field)
  - Advanced sensor health scoring ✅ COMPLETED v2.14.0

- [x] **Modular Sensor Architecture** ✅ COMPLETED v2.9.0 Phase 1

  - Abstract ISensor interface ✅
  - SensorManager for multi-sensor coordination ✅
  - Variable-length packet support (up to 16 values) ✅
  - ThermistorSensor implementation ✅
  - Backward compatible with legacy packets ✅
  - JST connector pin assignments ✅
  - I2C bus scanning with auto-detection ✅

  **Completed I2C Sensors:** ✅ COMPLETED v2.15.0

  - [x] BME680 (temp/humidity/pressure/gas) ✅
  - [x] BH1750 light sensor ✅
  - [x] INA219 current/power sensor ✅
  - [x] I2C auto-detection and initialization ✅
  - [x] Multi-sensor MQTT publishing ✅
  - [x] Home Assistant multi-sensor discovery ✅
  - [x] Sensor wiring documentation ✅

  **Still TODO for Additional Sensors:**

  - [ ] DS18B20 1-Wire temperature - Phase 3
  - [ ] DHT22 humidity sensor - Phase 4
  - [ ] Additional ADC sensors (photoresistor, soil moisture) - Phase 5
  - [ ] Web configuration interface for sensor settings - Phase 6
  - [ ] Motion detection (PIR)
  - [ ] Door/window contact sensors

- [ ] **Sensor Calibration**
  - Web-based calibration interface
  - Multi-point calibration for temperature
  - Battery voltage calibration
  - Offset and gain adjustments
  - Calibration history/versioning

### Display Enhancements

- [x] **Enhanced Display System** ✅ COMPLETED v2.0.0

  - Inverse headers (white-on-black title bars) ✅
  - Fixed signal graph boundaries ✅
  - Base station: 5 pages (Status/Sensors/Stats/Signal/Battery) ✅
  - Sensor: 3 pages (Status/TX Stats/Battery) ✅
  - 5-second auto-advance between pages ✅
  - 5-minute timeout with button wake ✅
  - QR code display with portrait rotation ✅
  - Multi-click button controls ✅

- [ ] **Customizable Display Pages** 🔵 FUTURE

  - User-configurable page layout
  - Show/hide specific pages
  - Adjustable refresh rates per page
  - Custom display themes
  - Font size options
  - Brightness control (day/night modes)

- [ ] **Advanced Graphs**
  - Temperature trend graphs (hourly, daily)
  - Battery discharge curves
  - RSSI heat maps
  - Multi-sensor comparison graphs
  - Min/max/average indicators

### Network & Communication

- [x] **Network Pairing & Security** ✅ COMPLETED

  **Phase 1** - ✅ COMPLETED v2.12.0, TESTED v2.12.2

  - Network ID (1-65535) user-configurable ✅
  - LoRa sync word calculation: 0x12 + (networkId % 244) ✅
  - Hardware filtering via Radio.SetSyncWord() ✅
  - Software validation: networkId in packet headers ✅
  - Captive portal Network ID configuration ✅
  - NVS persistence ✅
  - Sensor display shows ID + Network ID ✅
  - **Fully tested and validated** ✅

  **Phase 2** - ✅ COMPLETED v2.13.0

  - AES-128-CBC encryption with mbedTLS ✅
  - Random IV generation per packet ✅
  - HMAC authentication (8-byte) ✅
  - Replay attack prevention (sequence numbers) ✅
  - Device whitelist (up to 32 devices) ✅
  - Security management web UI (/security) ✅
  - Encryption key management (view/copy/generate/set) ✅
  - Sensor configuration with encryption key ✅
  - Display security indicator ([E]) ✅
  - Backward compatible (encrypted + unencrypted coexist) ✅
  - **Fully tested and validated** ✅

- [ ] **Mesh Network Support**
  - Sensor-to-sensor relay capability
  - Extended range through hopping
  - Self-healing network topology
  - Automatic route optimization
  - Network health monitoring

## 🟢 Low Priority - Nice-to-Have Features

### Security

- [x] **Authentication & Authorization** ✅ PARTIAL (Encryption implemented v2.13.0)

  - Encrypted LoRa communications ✅
  - Password-protected web interface (TODO)
  - API key authentication (TODO)
  - User roles (admin, viewer) (TODO)
  - Session management (TODO)
  - HTTPS/TLS support (TODO)

### Automation & Integration

- [x] **Home Assistant Integration** ✅ COMPLETED v2.8.0

  - MQTT auto-discovery ✅
  - Entity creation for each sensor ✅
  - Temperature/battery/RSSI sensors ✅
  - Device grouping by sensor ID ✅
  - Proper device classes and units ✅

- [ ] **IFTTT Integration**

  - Webhook triggers
  - Applet creation guides
  - Multi-condition triggers
  - Action logging

- [ ] **Rules Engine**
  - If-this-then-that logic
  - Multiple conditions (AND/OR)
  - Time-based rules
  - Hysteresis support
  - Rule testing/simulation

### Diagnostics & Maintenance

- [ ] **Advanced Diagnostics**

  - Packet error rate (PER) tracking
  - Link quality index (LQI)
  - Time on air calculations
  - Spectrum analyzer
  - Interference detection
  - Collision detection

- [ ] **Remote Management**
  - Remote reboot capability
  - Remote configuration changes
  - Firmware rollback
  - Log file retrieval
  - Factory reset option

### User Experience

- [ ] **Mobile App**

  - Native iOS/Android apps
  - Push notifications
  - Real-time monitoring
  - Sensor configuration
  - Alert management
  - Offline mode with sync

- [ ] **Voice Assistant Integration**
  - Alexa skill
  - Google Assistant action
  - Voice queries for sensor status
  - Voice alerts

### Data Analysis

- [ ] **Analytics Dashboard**

  - Sensor uptime statistics
  - Battery life predictions
  - Temperature patterns/anomalies
  - Correlation analysis
  - Export analytics reports

- [ ] **Machine Learning**
  - Predictive maintenance
  - Anomaly detection
  - Battery life prediction
  - Optimal transmission timing
  - Automatic threshold tuning

## 🔵 Future Considerations

### Hardware Expansion

- [ ] GPS location tracking for mobile sensors
- [ ] External antenna support
- [ ] Multiple radio support (868/915/433 MHz)
- [ ] E-ink display option (ultra-low power)

### Protocol Support

- [ ] BLE mesh for local sensors
- [ ] Zigbee gateway mode
- [ ] Z-Wave compatibility
- [ ] Matter protocol support

### Advanced Features

- [ ] Time-series database optimization
- [ ] Edge computing/local AI processing
- [ ] Professional weather station mode

---

## ✅ Completed Sprint - WiFi Captive Portal Implementation (v2.0.0)

### Phase 1: Core Captive Portal ✅ COMPLETED

- [x] Create AP mode initialization
- [x] Implement DNS server for captive portal redirect
- [x] Build responsive HTML portal page
- [x] Add mode selection (Sensor vs Base Station)
- [x] NVS storage for configuration

### Phase 2: Sensor Client Configuration ✅ COMPLETED

- [x] Sensor ID input and validation (1-255)
- [x] Location/name text field
- [x] Transmission interval selector (15s/30s/60s/300s)
- [x] Save config and reboot logic
- [x] Display configuration on OLED

### Phase 3: Base Station Configuration ✅ COMPLETED

- [x] WiFi network scanning (300ms timeout)
- [x] Network list presentation in portal
- [x] Password input handling (open network support)
- [x] Connection testing with retry logic
- [x] Fallback to AP mode on failure
- [x] Display IP address on OLED

### Phase 4: Enhanced Features ✅ COMPLETED

- [x] Factory reset via button hold (5 seconds)
- [x] LED status indicators
- [x] QR code for easy AP connection (portrait mode)
- [x] Multi-click button detection (5 actions)
- [x] Immediate ping functionality (double click)
- [x] Sensor timeout monitoring (15-minute threshold)
- [x] Base station battery monitoring page
- [x] Inverse display headers
- [x] Fixed display graph boundaries
- [x] Error handling and user feedback
- [x] Field testing completed

### Phase 5: Documentation ✅ COMPLETED

- [x] Comprehensive CHANGELOG.md
- [x] Updated README.md with setup guide
- [x] Captive portal instructions
- [x] Button control documentation
- [x] Troubleshooting guide

---

## 🔵 Current Focus - Phase 3: Advanced Features

### Recently Completed (v2.16.0):

- [x] **LoRa Settings Coordinated Reboot** ✅ COMPLETED
- [x] ACK tracking for all sensors before base station reboot ✅
- [x] Real-time progress monitoring in web UI ✅
- [x] 20-second timeout with partial ACK handling ✅
- [x] `/api/lora/reboot-status` endpoint for live status ✅
- [x] Frontend modal with per-sensor ACK display ✅
- [x] Sensors auto-reboot 5s after ACK, base 8s after all ACKs ✅
- [x] **UNBLOCKED**: /lora-settings page now fully functional ✅

### Previous Milestones (v2.15.0-v2.15.3):

- [x] I2C Sensor Implementations (BME680, BH1750, INA219) ✅
- [x] UI Standardization (semantic HTML, pico-custom.css) ✅
- [x] Runtime Config UX (loading states, auto-refresh) ✅
- [x] Location sync between sensor and base station ✅
- [x] Display improvements (welcome pages, 10s cycle) ✅

### Next Priority Options:

1. **Hardware Testing with Real Sensors** (Low effort, high validation) ⭐⭐⭐ HIGHEST PRIORITY

   - [ ] Test BME680 with actual hardware (ordered, arriving soon)
   - [ ] Test BH1750 light sensor
   - [ ] Test INA219 power monitor
   - [ ] Validate multi-sensor packets over LoRa
   - [ ] Verify MQTT publishing with all sensor types
   - [ ] Test adaptive font sizing with various names
   - [ ] Test LoRa settings changes with multiple sensors
   - **VALIDATES**: All recent code (I2C sensors, UI improvements, LoRa sync)

2. **Mesh Network Testing** (Low effort, high value) ⭐⭐

   - [ ] Code complete (v3.0.0), just needs 3-node testing
   - [ ] Validate route discovery and forwarding
   - [ ] Test self-healing on node failure
   - [ ] Measure range extension through relay
   - [ ] Web dashboard mesh status page
   - **ADVANTAGE**: Already implemented, just needs validation

3. **Additional Sensor Types** (Medium effort, medium value)
   - [ ] DS18B20 1-Wire temperature (Phase 3)
   - [ ] DHT22 humidity sensor (Phase 4)
   - [ ] Additional ADC sensors (photoresistor, soil moisture) (Phase 5)
   - [ ] Change sensor location names
   - [ ] Modify alert thresholds
   - [ ] LoRa parameters tuning (via #1 above)
   - **DEPENDS ON**: #1 for LoRa settings

**Shelved/Deferred:**

- ❌ Deep Sleep - Lab use with constant power, not needed
- ❌ SD Card Logging - No SD card reader available
- ❌ SMS Alerts - Deferred for later
- ❌ OTA Firmware Updates - USB updates preferred for now

### Planned Features:

- [ ] Full web server on base station (beyond captive portal) ✅ BASIC VERSION DONE
- [ ] Real-time sensor dashboard with live charts ✅ DONE (polling)
- [ ] Historical data graphs (hourly/daily/weekly)
- [x] Configuration page for runtime settings ✅ COMPLETED v2.15.0
- [ ] WebSocket for live updates (currently using polling)
- [ ] Export data (CSV/JSON) ✅ DONE
- [ ] Alert configuration interface
- [ ] OTA firmware update interface

---

## Implementation Priority Matrix

| Feature Category            | Complexity | Impact       | Priority       | Status                           |
| --------------------------- | ---------- | ------------ | -------------- | -------------------------------- |
| WiFi Captive Portal         | Medium     | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.0.0              |
| Sensor Health Monitoring    | Low        | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.0.0              |
| Display Enhancements        | Low        | Medium       | ⭐⭐⭐⭐       | ✅ Completed v2.0.0              |
| Multi-Click Buttons         | Low        | Medium       | ⭐⭐⭐⭐       | ✅ Completed v2.0.0              |
| Web Dashboard (Basic)       | Medium     | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.1.0              |
| Teams Notifications         | Low        | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.3.0              |
| Email Alerts Phase 1\*\*    | **Medium** | **High**     | **⭐⭐⭐⭐⭐** | **✅ Completed v2.12.0**         |
| **Network Pairing Phase 2** | Low        | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.5.0              |
| WebSocket Live Updates      | Low        | Medium       | ⭐⭐⭐⭐       | ✅ Completed v2.6.0              |
| In-Memory Historical Data   | Medium     | High         | ⭐⭐⭐⭐       | ✅ Completed v2.7.0              |
| MQTT Publishing             | Low        | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.8.0              |
| Home Assistant Integration  | Low        | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.8.0              |
| Modular Sensor Architecture | Medium     | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.9.0              |
| Remote LoRa Configuration   | Medium     | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.10.0             |
| Network Pairing/Security    | Medium     | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.13.0             |
| I2C Sensor Implementations  | Low        | High         | ⭐⭐⭐⭐⭐     | ✅ Completed v2.15.0             |
| Display Enhancements        | Low        | Medium       | ⭐⭐⭐⭐       | ✅ Completed v2.15.0             |
| UI Standardization/UX       | Low        | Medium       | ⭐⭐⭐⭐       | ✅ Completed v2.15.3             |
| **LoRa Settings Sync**      | **Medium** | **CRITICAL** | **⭐⭐⭐⭐⭐** | **✅ Completed v2.16.0**         |
| **Hardware Sensor Testing** | **Low**    | **High**     | **⭐⭐⭐⭐⭐** | **🟡 RECOMMENDED NEXT**          |
| **Mesh Network Testing**    | **Low**    | **High**     | **⭐⭐⭐⭐**   | **🟢 READY FOR TESTING**         |
| **Hardware Sensor Testing** | **Low**    | **High**     | **⭐⭐⭐⭐⭐** | **🟡 RECOMMENDED NEXT**          |
| **Mesh Network Testing**    | **Low**    | **High**     | **⭐⭐⭐⭐**   | **🟢 READY FOR TESTING**         |
| **Runtime Config Web UI**   | **Medium** | **Medium**   | **⭐⭐⭐⭐**   | **🔵 FUTURE**                    |
| Deep Sleep Mode             | Medium     | High         | ⭐⭐⭐⭐       | ⚪ Shelved (lab use, not remote) |
| OTA Firmware Updates        | Medium     | High         | ⭐⭐⭐⭐⭐     | ⚪ Shelved (USB preferred)       |
| SMS Alerts (Twilio)         | Low        | Medium       | ⭐⭐⭐         | ⚪ Shelved (deferred)            |
| SD Card Logging             | Low        | Medium       | ⭐⭐⭐⭐       | ⚪ Shelved (no hardware)         |
| Mobile App                  | Very High  | Medium       | ⭐⭐           | ⚪ Future                        |

---

## Detailed Version History

### v2.15.0 (December 14, 2025) - CURRENT

- ✅ I2C Sensor Support: BME680, BH1750, INA219
- ✅ Multi-sensor MQTT publishing
- ✅ Home Assistant multi-sensor auto-discovery
- ✅ Sensor wiring documentation (SENSOR_WIRING.md)
- ✅ Welcome pages with adaptive fonts
- ✅ Page cycle increased to 10 seconds
- ✅ Button action swap (2=ping, 3=reboot)
- ✅ LoRa settings page UI complete
- ⚠️ LoRa settings sync protocol incomplete (coordinated reboot)

### v2.14.0 (December 14, 2025)

- ✅ Display Enhancement: Sensor status page (1/3) shows sensor ID and network ID
- ✅ UX Consistency: All web pages standardized to 800px width
- ✅ Performance: Alerts page moved from embedded HTML to LittleFS
- ✅ Bug Fixes: JavaScript syntax errors in /sensors page
- ✅ Terminology: Client configuration page labels corrected
- ✅ Network Pairing Phase 1: Fully tested and validated
- ✅ Memory Optimization: ~20KB freed by moving HTML to filesystem

### v2.12.1 (December 13, 2025)

- ✅ Base station captive portal watchdog fix
- ✅ Dashboard "Forget" button for inactive clients
- ✅ Client location display improvements

### v2.12.0 (December 13, 2025)

- ✅ Network Pairing Phase 1 implementation
- ✅ Code quality tools setup

### v2.10.0 (December 13, 2025)

### v2.10.0 (December 13, 2025) - CURRENT

- ✅ Remote Configuration via LoRa (SET_INTERVAL command)
- ✅ Command retry system (3 attempts, 10s timeout)
- ✅ Piggyback ACK in telemetry headers
- ✅ Immediate ACK transmission after command processing
- ✅ Sensor continuous RX mode
- ✅ HTTP API endpoint (/api/remote-config/interval)
- ✅ Base station dashboard fix

### v2.9.0 (December 13, 2025)

- ✅ Modular sensor architecture with abstract ISensor interface
- ✅ SensorManager class for multi-sensor coordination
- ✅ Variable-length MultiSensorPacket (up to 16 values)
- ✅ ThermistorSensor concrete implementation
- ✅ Backward compatible with legacy packets
- ✅ I2C bus scanning with auto-detection
- ✅ JST connector pin assignments (4-pin I2C, 3-pin OneWire/DHT/ADC)
- ✅ Sensor library dependencies prepared for Phase 2-5

### v2.8.0 (December 13, 2025)

- ✅ MQTT Publishing with PubSubClient
- ✅ Home Assistant auto-discovery
- ✅ Individual + JSON state topics
- ✅ QoS support (0, 1, 2)
- ✅ Auto-reconnect with exponential backoff
- ✅ Web configuration interface (/mqtt)
- ✅ Connection statistics and testing

### v2.7.0 (December 13, 2025)

- ✅ Historical data graphs with Chart.js
- ✅ Multiple time ranges (1h/6h/12h/24h/7d/30d)
- ✅ Interactive graph hover tooltips
- ✅ Temperature history visualization

### v2.6.0 (December 13, 2025)

- ✅ WebSocket real-time updates
- ✅ Instant sensor data push to browsers

### v2.11.0 Notes:

**Problem:** Base station rebooting after restoring full dashboard HTML (730-line string exhausting ESP32 RAM ~20KB+)

**Solution:**

- Migrated to LittleFS filesystem with separate HTML/CSS/JS files
- Fixed fundamental architecture flaw: conflating clients (devices) with sensors (probes)
- Proper separation: ClientInfo tracks device telemetry (battery/RSSI/charging), PhysicalSensor tracks readings
- Independent history buffers (100 entries each)
- API now returns client-only data, sensors will have separate endpoints
- Dashboard shows only available data (hides empty charts)

**Impact:**

- RAM: 33.1% (108424 bytes) - stable
- Flash: 42.9% (1433941 bytes)
- No crashes after hours of operation
- System ready for DS18B20 temperature sensor and future multi-sensor support

### v2.5.0 (December 12, 2025)

- ✅ Email notifications via SMTP
- ✅ HTML formatted email alerts
- ✅ SMTP configuration (server, port, credentials, TLS)
- ✅ Test email functionality
- ✅ Dual-channel alerts (Teams + Email)
- ✅ Multiple recipient support

### v2.4.0 (December 12, 2025)

- ✅ Client terminology updates (Sensor → Client)
- ✅ Client inactivity timeout configuration
- ✅ Visual dashboard warnings for inactive clients
- ✅ Configurable timeout thresholds

### v2.3.0 (December 12, 2025)

- ✅ Microsoft Teams webhook integration
- ✅ Alert configuration via web interface
- ✅ Temperature threshold alerts
- ✅ Battery low alerts (<20%)
- ✅ Client offline alerts
- ✅ Alert rate limiting (15-minute cooldown)
- ✅ Test webhook functionality

### v2.2.0 (December 12, 2025)

- ✅ Alerts page and configuration system
- ✅ Temperature threshold settings
- ✅ Client inactivity detection
- ✅ Alert enable/disable controls

### v2.1.0 (December 12, 2025)

- ✅ Web dashboard with real-time sensor monitoring
- ✅ Auto-refresh system (5-second intervals)
- ✅ API endpoints (JSON sensor data, statistics)
- ✅ CSV/JSON data export
- ✅ Responsive design for mobile devices
- ✅ Color-coded status indicators

### v2.0.0 (December 12, 2025)

- ✅ WiFi captive portal with QR codes
- ✅ Dynamic configuration (no hardcoded values)
- ✅ Multi-click button system (5 actions)
- ✅ Sensor health monitoring (15-min timeout)
- ✅ Enhanced displays (inverse headers, fixed graphs)
- ✅ Base station battery monitoring
- ✅ Factory reset functionality
- ✅ Comprehensive documentation

### v1.0.0 (December 9, 2025)

- ✅ Basic LoRa communication (sensors → base station)
- ✅ Temperature and battery monitoring
- ✅ OLED display with multi-page cyclingWebSocket Updates or OTA Firmware
- ✅ WS2812 LED status indicators
- ✅ Statistics tracking
- ✅ Signal strength graphing

---10.0 (production)  
**Status**: v2.11.0 - Architecture Refactor Complete + Multi-Sensor Ready  
**Environment**: Lab Phase 2** ⭐ HIGHEST PRIORITY - Encryption (AES-128), whitelist, enhanced security 2. **Additional Sensor Types** - DS18B20 temperature (hardware arriving), BME680 environmental, BH1750 light 3. **Runtimapabilities:\*\*

- ✅ Real-time sensor monitoring via WebSocket
- ✅ Historical data visualization (Chart.js)
- ✅ MQTT publishing with Home Assistant integration
- ✅ Modular sensor architecture (plug-and-play JST connectors)
- ✅ Variable-length packets (up to 16 sensor values)
- ✅ Remote configuration via LoRa (no WiFi needed)
- ✅ Command retry system with automatic ACK
- ✅ Backward compatible with legacy devices
- ✅ Dual-channel alerts (Teams + Email)
- ✅ Web-based configuration and monitoring
- ✅ CSV/JSON data export
- ✅ LittleFS filesystem (stable, no memory issues)
- ✅ Client/Sensor separation (proper data model)
- ✅ Independent history tracking (device vs sensors)

**Recommended Next Steps (Lab Use):**

1. **LoRa Settings Sync** 🔴 BLOCKING - Complete coordinated reboot protocol for changing radio parameters
2. **Hardware Testing** 🟡 VALIDATE - Test BME680, BH1750, INA219 with actual hardware (arriving next week)
3. **Mesh Network Testing** 🟢 READY - 3-node topology testing, code complete (v3.0.0)
4. **Runtime Config Web UI** 🔵 FUTURE - Web interface for all remote commands
5. **Additional Sensor Types** 🔵 FUTURE - DS18B20 1-Wire temperature (Phase 3)
6. **Cloud Data Storage** 🔵 FUTURE - InfluxDB/ThingSpeak for long-term analytics
