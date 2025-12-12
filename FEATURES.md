# LoRa Sensor Station - Feature Roadmap

## 🔴 High Priority - Core Functionality

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

- [ ] **Web Dashboard** 🔴 PHASE 2 (Not Started)
  - Real-time sensor data dashboard
  - Historical data graphs
  - Configuration page for all settings
  - OTA (Over-The-Air) firmware updates
  - Network diagnostics page
  - Export data as CSV/JSON
  - WebSocket live updates
  
  **Note:** Base station currently serves captive portal for initial setup only. Full web dashboard planned for Phase 2.

### Alerts & Notifications
- [ ] **Microsoft Teams Integration**
  - Webhook configuration via web interface
  - Alert on temperature thresholds
  - Alert on battery low (<20%)
  - Alert on sensor offline (timeout)
  - Alert on communication failures
  - Customizable alert templates
  - Rate limiting to prevent spam

- [ ] **Email Notifications**
  - SMTP configuration (Gmail, Outlook, etc.)
  - HTML formatted email alerts
  - Attachment support (logs, graphs)
  - Multiple recipient support
  - Email digest (daily/weekly summaries)
  - Test email functionality

- [ ] **SMS Gateway Integration**
  - Twilio API integration
  - Critical alerts only (battery critical, sensor lost)
  - Phone number whitelist
  - SMS rate limiting (cost control)
  - Delivery confirmation

### Data Logging & Storage
- [ ] **SD Card Logging**
  - Timestamped sensor readings
  - CSV format for easy analysis
  - Automatic log rotation
  - Log file size limits
  - Error logging
  - Export via web interface

- [ ] **Cloud Data Storage**
  - InfluxDB integration
  - ThingSpeak support
  - MQTT broker publishing
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

- [ ] **Additional Sensor Types**
  - Humidity sensors (DHT22, SHT31)
  - Pressure sensors (BMP280, BME680)
  - Light intensity (BH1750)
  - Motion detection (PIR)
  - Door/window contact sensors
  - Soil moisture sensors
  - Current sensors (INA219)

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
- [ ] **Mesh Network Support**
  - Sensor-to-sensor relay capability
  - Extended range through hopping
  - Self-healing network topology
  - Automatic route optimization
  - Network health monitoring

- [ ] **LoRaWAN Integration**
  - TTN (The Things Network) support
  - ChirpStack compatibility
  - LoRaWAN device provisioning
  - Downlink command support
  - Adaptive data rate (ADR)

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
- [ ] **Home Assistant Integration**
  - MQTT discovery
  - Entity creation for each sensor
  - Custom cards for dashboard
  - Automation triggers
  - State restoration

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

## 🔵 Current Focus - Phase 2: Web Dashboard

### Planned Features:
- [ ] Full web server on base station (beyond captive portal)
- [ ] Real-time sensor dashboard with live charts
- [ ] Historical data graphs (hourly/daily/weekly)
- [ ] Configuration page for runtime settings
- [ ] WebSocket for live updates
- [ ] Export data (CSV/JSON)
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
| Web Dashboard | High | High | ⭐⭐⭐⭐⭐ | 🔴 Phase 2 |
| Teams Notifications | Low | High | ⭐⭐⭐⭐⭐ | 🔴 Phase 2 |
| Email Alerts | Low | High | ⭐⭐⭐⭐⭐ | 🔴 Phase 2 |
| SD Card Logging | Low | Medium | ⭐⭐⭐⭐ | ⚪ Planned |
| Deep Sleep | Medium | High | ⭐⭐⭐⭐ | ⚪ Planned |
| SMS Alerts | Low | Medium | ⭐⭐⭐ | ⚪ Planned |
| Multi-Sensor Expansion | Low | Medium | ⭐⭐⭐ | 🟡 Partial (10 sensors) |
| MQTT Integration | Medium | Medium | ⭐⭐⭐ | ⚪ Planned |
| Mobile App | Very High | Medium | ⭐⭐ | ⚪ Future |

---

## Version History

### v2.0.0 (December 12, 2025) - Current Release
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
- ✅ OLED display with multi-page cycling
- ✅ WS2812 LED status indicators
- ✅ Statistics tracking
- ✅ Signal strength graphing

---

**Last Updated**: December 12, 2025  
**Current Version**: v2.0.0  
**Status**: Phase 1 Complete - Phase 2 (Web Dashboard) Planning
