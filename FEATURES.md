# LoRa Sensor Station - Feature Roadmap

## Version History

### v2.12.1 (December 13, 2025) - Current Version
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
  - [ ] Additional command types (location, thresholds, mesh config)
  - [ ] Web UI for remote configuration
  - [ ] Command history logging
  - [ ] Batch commands (multiple sensors)

### WiFi Configuration & Management
- [x] **Captive Portal for WiFi Setup** ✅ COMPLETED v2.0.0
  
  **Sensor Client Path:** ✅ IMPLEMENTED
  1. Device boots in AP mode (SSID: "LoRa-Sensor-XXXX") ✅
  2. User connects phone/laptop to AP (password: "configure") ✅
  3. Captive portal automatically opens (DNS redirect) ✅
  4. User enters configuration: ✅
     - Unique Sensor ID (1-255) ✅
     - Sensor Location/Name (text field) ✅
     - Transmission Interval (15s/30s/60s/5min) ✅
  5. Save → Device stores config in NVS ✅
  6. Device reboots into sensor mode (LoRa only, no WiFi) ✅
  7. Display shows sensor status with configured settings ✅
  
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
  - [ ] Configuration page for runtime settings
  - [ ] OTA firmware update interface
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

- [ ] **Multi-Sensor Support**
  - Support for 20+ sensors (currently 10) ✅
  - Sensor grouping/zones
  - Sensor priority levels
  - Auto-discovery of new sensors ✅ (basic implementation)
  - Sensor naming/labeling via portal ✅ (location field)
  - Advanced sensor health scoring

- [x] **Modular Sensor Architecture** ✅ COMPLETED v2.9.0 Phase 1
  - Abstract ISensor interface ✅
  - SensorManager for multi-sensor coordination ✅
  - Variable-length packet support (up to 16 values) ✅
  - ThermistorSensor implementation ✅
  - Backward compatible with legacy packets ✅
  - JST connector pin assignments ✅
  - I2C bus scanning with auto-detection ✅
  
  **Still TODO for Full Sensor Support:**
  - [ ] BME680 (temp/humidity/pressure/gas) - Phase 2
  - [ ] BH1750 light sensor - Phase 2
  - [ ] INA219 current/power sensor - Phase 2
  - [ ] DS18B20 1-Wire temperature - Phase 3
  - [ ] DHT22 humidity sensor - Phase 4
  - [ ] Additional ADC sensors (photoresistor, soil moisture) - Phase 5
  - [ ] Web configuration interface - Phase 6
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
- [ ] **Network Pairing & Security** ⭐ HIGH PRIORITY
  - Unique network ID (prevent interference from other LoRa networks)
  - Device pairing/authentication
  - Encrypted payloads (AES-128)
  - Whitelist of allowed sensor IDs
  - Network ID configuration in captive portal
  - Sync word customization per network
  - Reject packets from unknown networks

- [ ] **Mesh Network Support**
  - Sensor-to-sensor relay capability
  - Extended range through hopping
  - Self-healing network topology
  - Automatic route optimization
  - Network health monitoring


## 🟢 Low Priority - Nice-to-Have Features

### Security
- [ ] **Authentication & Authorization**
  - Password-protected web interface
  - API key authentication
  - User roles (admin, viewer)
  - Session management
  - HTTPS/TLS support
  - Encrypted LoRa communications

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
- [ ] Waterproof enclosure designs

### Protocol Support
- [ ] BLE mesh for local sensors
- [ ] Zigbee gateway mode
- [ ] Z-Wave compatibility
- [ ] Matter protocol support

### Advanced Features
- [ ] Time-series database optimization
- [ ] Edge computing/local AI processing
- [ ] Blockchain logging (immutable audit trail)
- [ ] Satellite backup communication (emergency)
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
- [x] Immediate ping functionality (triple click)
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

### Recently Completed (v2.9.0 Phase 1):
- [x] Modular Sensor Architecture ✅
- [x] Abstract ISensor interface (12 sensor types) ✅
- [x] SensorManager with I2C auto-detection ✅
- [x] Variable-length MultiSensorPacket ✅
- [x] ThermistorSensor with Steinhart-Hart ✅
- [x] Backward compatible packet handling ✅

### Next Priority Options:
1. **I2C Sensor Implementations** (Medium effort, high value) ⭐ RECOMMENDED
   - [ ] BME680 environmental sensor (temp/humidity/pressure/gas) - Phase 2
   - [ ] BH1750 light intensity sensor - Phase 2
   - [ ] INA219 current/power monitoring - Phase 2
   - [ ] Web sensor configuration interface - Phase 6
   - Ready architecture, just need concrete implementations
   
2. **Runtime Configuration Page** (Medium effort, high value)
   - [ ] Adjust transmission intervals without reset
   - [ ] Change sensor location names
   - [ ] Modify alert thresholds
   - [ ] WiFi network changes
   - [ ] LoRa parameters tuning
   
3. **Cloud Data Storage** (Medium effort, medium value)
   - [ ] InfluxDB integration via MQTT
   - [ ] ThingSpeak direct publishing
   - [ ] Long-term data retention
   - [ ] Advanced analytics and queries

**Shelved/Deferred:**
- ❌ Deep Sleep - Lab use with constant power, not needed
- ❌ SD Card Logging - No SD card reader available
- ❌ SMS Alerts - Deferred for later
- ❌ OTA Firmware Updates - USB updates preferred for now

### Planned Features:
- [ ] Full web server on base station (beyond captive portal) ✅ BASIC VERSION DONE
- [ ] Real-time sensor dashboard with live charts ✅ DONE (polling)
- [ ] Historical data graphs (hourly/daily/weekly)
- [ ] Configuration page for runtime settings
- [ ] WebSocket for live updates (currently using polling)
- [ ] Export data (CSV/JSON) ✅ DONE
- [ ] Alert configuration interface
- [ ] OTA firmware update interface

---

## Implementation Priority Matrix

| Feature Category | Complexity | Impact | Priority | Status |
|-----------------|------------|--------|----------|--------|
| WiFi Captive Portal | Medium | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.0.0 |
| Sensor Health Monitoring | Low | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.0.0 |
| Display Enhancements | Low | Medium | ⭐⭐⭐⭐ | ✅ Completed v2.0.0 |
| Multi-Click Buttons | Low | Medium | ⭐⭐⭐⭐ | ✅ Completed v2.0.0 |
| Web Dashboard (Basic) | Medium | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.1.0 |
| Teams Notifications | Low | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.3.0 |
| Email Alerts | Low | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.5.0 |
| WebSocket Live Updates | Low | Medium | ⭐⭐⭐⭐ | ✅ Completed v2.6.0 |
| In-Memory Historical Data | Medium | High | ⭐⭐⭐⭐ | ✅ Completed v2.7.0 |
| MQTT Publishing | Low | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.8.0 |
| Home Assistant Integration | Low | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.8.0 |
| Modular Sensor Architecture | Medium | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.9.0 |
| Remote LoRa Configuration | Medium | High | ⭐⭐⭐⭐⭐ | ✅ Completed v2.10.0 |
| **Network Pairing/Security** | **Medium** | **High** | **⭐⭐⭐⭐⭐** | **🔵 NEXT RECOMMENDED** |
| **Additional Sensor Types** | **Low** | **High** | **⭐⭐⭐⭐⭐** | **🔵 ALTERNATIVE** |
| **Runtime Config Web UI** | **Low** | **Medium** | **⭐⭐⭐⭐** | **🔵 ALTERNATIVE** |
| Deep Sleep Mode | Medium | High | ⭐⭐⭐⭐ | ⚪ Shelved (lab use, not remote) |
| OTA Firmware Updates | Medium | High | ⭐⭐⭐⭐⭐ | ⚪ Shelved (USB preferred) |
| SMS Alerts (Twilio) | Low | Medium | ⭐⭐⭐ | ⚪ Shelved (deferred) |
| SD Card Logging | Low | Medium | ⭐⭐⭐⭐ | ⚪ Shelved (no hardware) |
| Mobile App | Very High | Medium | ⭐⭐ | ⚪ Future |

---

## Detailed Version History

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
**Environment**: Lab deployment with constant power, JST connector-based sensor expansion

**System Capabilities:**
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
1. **Network Pairing & Security** ⭐ HIGHEST PRIORITY - Prevent interference from other LoRa networks, unique network ID, device authentication
2. **Additional Sensor Types** - BME680 environmental, BH1750 light, INA219 power monitoring, DS18B20 temperature
3. **Remote Config Web UI** - Web interface for all remote commands (interval, location, thresholds, restart)
4. **Cloud Data Storage** - InfluxDB integration via MQTT, ThingSpeak publishing for long-term analytics
