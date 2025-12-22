#include "lora_comm.h"
#include "config.h"
#include "led_control.h"
#include "display_control.h"
#include "statistics.h"
#include "wifi_portal.h"
#include "sensor_interface.h"
#include "mesh_routing.h"
#include "config_storage.h"
#include "security.h"
#ifdef BASE_STATION
#include "mqtt_client.h"
#include "remote_config.h"
#endif
#ifdef SENSOR_NODE
#include "remote_config.h"
#include "buzzer.h"
#endif
#include <Arduino.h>
#include <sys/time.h>
#include "time_status.h"
#include "logger.h"

static RadioEvents_t RadioEvents;
static bool lora_idle = true;
static uint16_t currentNetworkId = 0;  // Current network ID for validation

#ifdef BASE_STATION
static bool pendingWebSocketBroadcast = false;  // Flag to trigger broadcast from main loop
static bool pendingCommandSend = false;  // Flag to send command from main loop
static uint8_t pendingCommandSensorId = 0;
static uint32_t pendingCommandReadyAtMs = 0;
static const uint32_t BASE_RX_TO_TX_HOLDDOWN_MS = 120;  // allow radio to settle after RX before TX
#endif

#ifdef SENSOR_NODE
uint8_t lastProcessedCommandSeq = 0;  // Last command sequence number we processed
uint8_t lastCommandAckStatus = 0;     // Status of last command (0=success, non-zero=error)
static bool pendingAckSend = false;   // Flag to send immediate telemetry with ACK
static uint32_t forcedIntervalUntil = 0;  // Timestamp until which to use forced 10s interval
static const uint32_t FORCED_INTERVAL_MS = 10000;  // 10 seconds
static const uint32_t FORCED_INTERVAL_DURATION = 30000;  // Keep forced interval for 30 seconds after command
static uint32_t ackFieldsValidUntil = 0;  // keep lastCommandSeq/ackStatus valid for a short window
#endif

// Calculate LoRa sync word from network ID
// Maps network ID (1-65535) to valid sync word range (0x12-0xFF)
uint8_t calculateSyncWord(uint16_t networkId) {
    return 0x12 + (networkId % 244);  // 0x12 to 0xFF (244 values)
}

// Forward declarations
#ifdef BASE_STATION
void sendCommandNow(uint8_t sensorId);

// Broadcast wake ping: wakes all listening sensors and shows "Cmd Recv'd".
// This is intentionally NOT queued/tracked for ACKs to avoid collisions.
void sendBroadcastWakePing() {
  CommandPacket cmd;
  cmd.syncWord = COMMAND_SYNC_WORD;
  cmd.commandType = CMD_PING;
  cmd.targetSensorId = 0xFF;  // broadcast
  cmd.sequenceNumber = 0;     // not tracked/acked
  cmd.dataLength = 0;
  memset(cmd.data, 0, sizeof(cmd.data));

  extern RemoteConfigManager remoteConfigManager;
  size_t checksumLength = sizeof(CommandPacket) - sizeof(uint16_t);
  cmd.checksum = remoteConfigManager.calculateChecksum((const uint8_t*)&cmd, checksumLength);

  // Preempt RX and transmit immediately.
  Radio.Standby();
  delay(20);
  Radio.Send((uint8_t*)&cmd, sizeof(CommandPacket));
  lora_idle = false;
  LOGI("CMD", "Broadcast wake ping sent (CMD_PING, target=0xFF)");
}
#endif

// Extern declarations

void initLoRa() {
  // Load network ID from configuration
  #ifdef BASE_STATION
    BaseStationConfig config = configStorage.getBaseStationConfig();
    currentNetworkId = config.networkId;
  #elif defined(SENSOR_NODE)
    SensorConfig config = configStorage.getSensorConfig();
    currentNetworkId = config.networkId;
  #endif
  
  uint8_t syncWord = calculateSyncWord(currentNetworkId);
  
  // Check for pending LoRa parameter changes (from SET_LORA_PARAMS command)
  uint32_t frequency = RF_FREQUENCY;
  uint8_t spreadingFactor = LORA_SPREADING_FACTOR;
  uint32_t bandwidth = LORA_BANDWIDTH;
  uint8_t txPower = TX_OUTPUT_POWER;
  uint8_t codingRate = LORA_CODINGRATE;
  
  Preferences prefs;
  bool roOK = prefs.begin("lora_params", true);  // Read-only
  if (!roOK) {
    // Initialize namespace if missing, then reopen read-only
    prefs.begin("lora_params", false);
    prefs.end();
    prefs.begin("lora_params", true);
  }
  
  // Always check if custom parameters are stored in NVS
  // If frequency is set in NVS (non-zero), use stored parameters
  uint32_t storedFreq = prefs.getUInt("frequency", 0);
  if (storedFreq != 0) {
    // Load parameters from NVS
    frequency = storedFreq;
    spreadingFactor = prefs.getUChar("sf", LORA_SPREADING_FACTOR);
    bandwidth = prefs.getUInt("bandwidth", LORA_BANDWIDTH);
    txPower = prefs.getUChar("tx_power", TX_OUTPUT_POWER);
    codingRate = prefs.getUChar("coding_rate", LORA_CODINGRATE);
    
    // Validate bandwidth (must be at least 10000 Hz)
    if (bandwidth < 10000) {
      LOGW("LORA", "Invalid bandwidth: %u Hz; using default %u Hz", bandwidth, LORA_BANDWIDTH);
      bandwidth = LORA_BANDWIDTH;
      
      // Write corrected value back to NVS
      prefs.end();
      prefs.begin("lora_params", false);  // Read-write
      prefs.putUInt("bandwidth", bandwidth);
      prefs.end();
      prefs.begin("lora_params", true);  // Back to read-only
      LOGI("LORA", "Corrected bandwidth saved to NVS");
    }
    
    LOGI("LORA", "LOADING LORA PARAMETERS FROM NVS");
    LOGI("LORA", "Frequency: %u Hz", frequency);
    LOGI("LORA", "Spreading Factor: SF%d", spreadingFactor);
    LOGI("LORA", "Bandwidth: %u Hz", bandwidth);
    LOGI("LORA", "TX Power: %d dBm", txPower);
    LOGI("LORA", "Coding Rate: %d", codingRate);
  } else {
    LOGI("LORA", "USING DEFAULT LORA PARAMETERS");
    LOGI("LORA", "Frequency: %u Hz", frequency);
    LOGI("LORA", "Spreading Factor: SF%d", spreadingFactor);
    LOGI("LORA", "Bandwidth: %u Hz", bandwidth);
    LOGI("LORA", "TX Power: %d dBm", txPower);
    LOGI("LORA", "Coding Rate: %d", codingRate);
  }
  
  // Check if there's a pending parameter update that needs confirmation
  bool wasPending = prefs.getBool("pending", false);
  prefs.end();
  
  // Clear pending flag if it was set (parameter change has been applied)
  if (wasPending) {
    prefs.begin("lora_params", false);  // Read-write
    prefs.putBool("pending", false);
    prefs.end();
    LOGI("LORA", "Pending LoRa parameter update applied and confirmed");
  }
  
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(frequency);
  
  // Set hardware sync word for network isolation (SX1262 specific)
  // Note: SX1262 uses 2-byte sync word, we use the calculated byte twice
  Radio.SetSyncWord((syncWord << 8) | syncWord);
  
  // Convert bandwidth from Hz to SX126x enum if needed
  // Note: If bandwidth is saved as 0/1/2 enum, use it directly
  // If saved as Hz (125000/250000/500000), convert to enum
  uint8_t bwEnum = bandwidth;
  if (bandwidth > 10) {
    // Convert Hz to enum
    if (bandwidth == 125000) bwEnum = 0;
    else if (bandwidth == 250000) bwEnum = 1;
    else if (bandwidth == 500000) bwEnum = 2;
  }
  
  Radio.SetTxConfig(
    MODEM_LORA, txPower, 0, bwEnum,
    spreadingFactor, codingRate,
    LORA_PREAMBLE_LENGTH, false,  // Changed to variable length
    true, 0, 0, LORA_IQ_INVERSION_ON, 3000
  );
  
  Radio.SetRxConfig(
    MODEM_LORA, bwEnum, spreadingFactor,
    codingRate, 0, LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT, false,  // Changed to variable length
    0, true, 0, 0, LORA_IQ_INVERSION_ON, true  // Max payload 0 = use variable length mode
  );
  
  #ifdef BASE_STATION
    LOGI("LORA", "Base ready; listening for sensors");
    LOGI("LORA", "Frequency: %u Hz", frequency);
    LOGI("LORA", "BW: %d, SF: %d, CR: %d", bwEnum, spreadingFactor, codingRate);
    LOGI("LORA", "Network ID: %d (Sync Word: 0x%02X)", currentNetworkId, syncWord);
    LOGI("LORA", "Expected packet size: %d bytes", sizeof(SensorData));
  #elif defined(SENSOR_NODE)
    LOGI("LORA", "Sensor ready; preparing to send");
    LOGI("LORA", "Network ID: %d (Sync Word: 0x%02X)", currentNetworkId, syncWord);
  #endif
}

bool isLoRaIdle() {
  return lora_idle;
}

void setLoRaIdle(bool idle) {
  lora_idle = idle;
}

void sendSensorData(const SensorData& data) {
  LOGI("TX", "Transmitting packet");
  
  recordTxAttempt();
  
  Radio.Sleep();
  delay(10);
  
  // Check if encryption is enabled
  if (securityManager.isEncryptionEnabled()) {
    LOGD("TX", "Encrypting packet...");
    EncryptedPacket encryptedPkt;
    uint16_t encLen = securityManager.encryptPacket(
      (const uint8_t*)&data, 
      sizeof(SensorData),
      &encryptedPkt,
      data.sensorId,
      data.networkId
    );
    
    if (encLen > 0) {
      LOGD("TX", "Encrypted packet size: %d bytes", encLen);
      Radio.Send((uint8_t*)&encryptedPkt, encLen);
    } else {
      LOGE("TX", "Encryption failed");
      return;
    }
  } else {
    LOGD("TX", "Packet size: %d bytes (unencrypted)", sizeof(SensorData));
    Radio.Send((uint8_t*)&data, sizeof(SensorData));
  }
  
  lora_idle = false;
}

void enterRxMode() {
  if (lora_idle) {
    lora_idle = false;
    LOGI("RX", "Entering RX mode");
    Radio.Rx(0);
  }
}

// ============================================================================
// RADIO CALLBACKS
// ============================================================================
void OnTxDone() {
  LOGI("TX", "TX Done - Packet sent successfully");
  recordTxSuccess();
  #ifdef SENSOR_NODE
    blinkLED(getColorBlue(), 2, 100);
    // Keep ACK fields alive briefly so the base has multiple chances to observe them.
    // They are cleared when the forced ACK window expires.
    if (lastProcessedCommandSeq != 0 && ackFieldsValidUntil != 0 && (int32_t)(millis() - ackFieldsValidUntil) >= 0) {
      lastProcessedCommandSeq = 0;
      lastCommandAckStatus = 0;
      ackFieldsValidUntil = 0;
      forcedIntervalUntil = 0;
    }
    // Go back to RX mode to listen for commands
    // Use Standby to properly reset radio state after TX
    Radio.Standby();
    delay(100);
    LOGD("RX", "Back to RX mode, listening for commands");
    Radio.Rx(0);
    lora_idle = true;  // Ready to receive
  #else
    Serial.println("DEBUG: Base station TX complete");
    lora_idle = true;
    Radio.Rx(0); // Continue listening for sensor data
  #endif
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  // Check for null payload first
  if (payload == nullptr) {
    LOGE("RX", "OnRxDone called with null payload");
    return;
  }
  
  // Ignore 0-byte packets (CRC errors or sync word mismatches)
  if (size == 0) {
    LOGW("RX", "Received 0 bytes (CRC error or sync mismatch) - ignoring");
    return;
  }
  
  #ifdef BASE_STATION
    LOGI("RX", "Received %d bytes, RSSI: %d, SNR: %d", size, rssi, snr);
    LOGD("RX", "Expected legacy size: %d bytes", sizeof(SensorData));
  #elif defined(SENSOR_NODE)
    LOGI("RX", "Received %d bytes, RSSI: %d, SNR: %d", size, rssi, snr);
  #endif
  
  #ifdef BASE_STATION
    Radio.Sleep();
    
    recordRxPacket(rssi);
    
    // Check if it's a command packet (sensor announcement)
    if (size >= sizeof(CommandPacket)) {
      CommandPacket* cmd = (CommandPacket*)payload;
      if (cmd->syncWord == COMMAND_SYNC_WORD && cmd->commandType == CMD_SENSOR_ANNOUNCE) {
        // Extract sensor ID from announcement payload
        uint8_t announcingSensorId = (cmd->dataLength > 0) ? cmd->data[0] : cmd->targetSensorId;
        LOGI("ANNOUNCE", "Sensor %d announced itself on startup", announcingSensorId);
        
        // Use existing time sync mechanism instead of direct send
        time_t now = time(nullptr);
        if (now > 1000000000) {  // Valid time
          NTPConfig ntpConfig = configStorage.getNTPConfig();
          uint8_t payload[6];
          memcpy(&payload[0], &now, sizeof(uint32_t));
          int16_t tz = ntpConfig.tzOffsetMinutes;
          memcpy(&payload[4], &tz, sizeof(int16_t));
          
          // Queue time sync command using the existing reliable mechanism
          extern RemoteConfigManager remoteConfigManager;
          if (remoteConfigManager.queueCommand(announcingSensorId, CMD_TIME_SYNC, payload, 6)) {
            LOGI("ANNOUNCE", "Queued time sync for sensor %d (epoch=%lu, tz=%d)", 
                 announcingSensorId, (unsigned long)now, (int)tz);
          } else {
            LOGW("ANNOUNCE", "Failed to queue time sync for sensor %d", announcingSensorId);
          }
        } else {
          LOGW("ANNOUNCE", "Cannot send time sync to sensor %d - NTP not synced", 
               announcingSensorId);
        }
        
        // Continue processing packet normally
        // Don't return here - let it fall through to regular packet processing
      }
    }
    
    // Only check for mesh packets if mesh is enabled
    BaseStationConfig baseConfig = configStorage.getBaseStationConfig();
    LOGD("MESH", "Mesh enabled: %s", baseConfig.meshEnabled ? "YES" : "NO");
    
    // Check if it's a mesh packet (first byte is MeshPacketType enum)
    if (baseConfig.meshEnabled && size >= sizeof(MeshHeader)) {
      MeshHeader* meshHdr = (MeshHeader*)payload;
      if (meshHdr->packetType >= MESH_DATA && meshHdr->packetType <= MESH_NEIGHBOR_BEACON) {
        LOGI("MESH", "Mesh packet detected, processing");
        meshRouter.processReceivedPacket(payload, size, rssi);
        
        // If it's a data packet for us, extract and process the payload
        if (meshHdr->packetType == MESH_DATA && 
            (meshHdr->destId == 1 || meshHdr->destId == 255)) {  // Base station ID = 1
          uint8_t* dataPayload = payload + sizeof(MeshHeader);
          uint16_t dataSize = size - sizeof(MeshHeader);
          
          // Re-process as sensor data packet
          if (dataSize >= sizeof(SensorData)) {
            SensorData received;
            memcpy(&received, dataPayload, sizeof(SensorData));
            
            if (received.syncWord == SYNC_WORD && 
                received.networkId == currentNetworkId && 
                validateChecksum(&received)) {
              Serial.println("\n=== MESH-ROUTED LEGACY PACKET ===");
              Serial.printf("Via %d hops from node %d\n", meshHdr->hopCount, meshHdr->sourceId);
              updateSensorInfo(received, rssi, snr);
              
              // Continue with normal MQTT publishing...
              SensorInfo* sensor = getSensorInfo(received.sensorId);
              if (sensor != NULL) {
                mqttClient.publishSensorData(
                  received.sensorId,
                  sensor->location,
                  received.temperature,
                  received.batteryPercent,
                  rssi,
                  snr
                );
                
                static bool discoveryPublished[256] = {false};
                if (!discoveryPublished[received.sensorId]) {
                  mqttClient.publishHomeAssistantDiscovery(received.sensorId, sensor->location);
                  discoveryPublished[received.sensorId] = true;
                }
              }
              
              if (wifiPortal.isDashboardActive()) {
                wifiPortal.broadcastSensorUpdate();
              }
              
              Serial.printf("Sensor ID: %d\n", received.sensorId);
              Serial.printf("Temperature: %.2f°C\n", received.temperature);
              Serial.printf("Battery Voltage: %.2fV\n", received.batteryVoltage);
              Serial.printf("Battery Percent: %d%%\n", received.batteryPercent);
              Serial.printf("RSSI: %d dBm\n", rssi);
              Serial.printf("SNR: %d dB\n", snr);
              Serial.println("====================\n");
              
              if (received.batteryPercent > 80) {
                blinkLED(getColorGreen(), 1, 200);
              } else if (received.batteryPercent > 50) {
                blinkLED(getColorYellow(), 1, 200);
              } else if (received.batteryPercent > 20) {
                blinkLED(getColorOrange(), 2, 200);
              } else {
                blinkLED(getColorRed(), 3, 200);
              }
              setLED(getColorGreen());
            }
          } else if (dataSize >= sizeof(MultiSensorHeader)) {
            // Handle mesh-routed multi-sensor packets
            MultiSensorPacket received;
            memcpy(&received, dataPayload, dataSize);
            
            if (received.header.packetType == PACKET_MULTI_SENSOR && 
                received.header.networkId == currentNetworkId && 
                validateMultiSensorChecksum(&received)) {
              Serial.println("\n=== MESH-ROUTED MULTI-SENSOR PACKET ===");
              Serial.printf("Via %d hops from node %d\n", meshHdr->hopCount, meshHdr->sourceId);
              
              // Process as normal multi-sensor packet...
              SensorData legacyData;
              legacyData.syncWord = SYNC_WORD;
              legacyData.networkId = received.header.networkId;
              legacyData.sensorId = received.header.sensorId;
              legacyData.batteryVoltage = 0.0f;
              legacyData.batteryPercent = received.header.batteryPercent;
              legacyData.powerState = received.header.powerState;
              // Copy location and zone from packet header
              strncpy(legacyData.location, received.header.location, sizeof(legacyData.location) - 1);
              legacyData.location[sizeof(legacyData.location) - 1] = '\0';
              strncpy(legacyData.zone, received.header.zone, sizeof(legacyData.zone) - 1);
              legacyData.zone[sizeof(legacyData.zone) - 1] = '\0';
              
              for (int i = 0; i < received.header.valueCount; i++) {
                if (received.values[i].type == VALUE_TEMPERATURE) {
                  legacyData.temperature = received.values[i].value;
                  break;
                }
              }
              
              updateSensorInfo(legacyData, rssi, snr);
              
              // Store individual sensor readings
              for (int i = 0; i < received.header.valueCount; i++) {
                updateSensorReading(received.header.sensorId, i, 
                                  received.values[i].type, received.values[i].value);
              }
              
              SensorInfo* sensor = getSensorInfo(received.header.sensorId);
              if (sensor != NULL) {
                // Use new multi-sensor MQTT publish function
                mqttClient.publishMultiSensorData(
                  received.header.sensorId,
                  sensor->location,
                  received.values,
                  received.header.valueCount,
                  received.header.batteryPercent,
                  rssi,
                  snr
                );
                
                static bool discoveryPublished[256] = {false};
                if (!discoveryPublished[received.header.sensorId]) {
                  mqttClient.publishHomeAssistantMultiSensorDiscovery(
                    received.header.sensorId,
                    sensor->location,
                    received.values,
                    received.header.valueCount
                  );
                  discoveryPublished[received.header.sensorId] = true;
                }
              }
              
              if (wifiPortal.isDashboardActive()) {
                pendingWebSocketBroadcast = true;  // Set flag instead of calling from ISR
              }
              
              Serial.printf("Sensor ID: %d\n", received.header.sensorId);
              Serial.printf("Value Count: %d\n", received.header.valueCount);
              for (int i = 0; i < received.header.valueCount; i++) {
                const char* typeName = "Unknown";
                switch(received.values[i].type) {
                  case VALUE_TEMPERATURE: typeName = "Temperature"; break;
                  case VALUE_HUMIDITY: typeName = "Humidity"; break;
                  case VALUE_PRESSURE: typeName = "Pressure"; break;
                  case VALUE_LIGHT: typeName = "Light"; break;
                  case VALUE_VOLTAGE: typeName = "Voltage"; break;
                  case VALUE_CURRENT: typeName = "Current"; break;
                  case VALUE_POWER: typeName = "Power"; break;
                  case VALUE_ENERGY: typeName = "Energy"; break;
                  case VALUE_GAS_RESISTANCE: typeName = "Gas Resistance"; break;
                  case VALUE_BATTERY: typeName = "Battery"; break;
                  case VALUE_SIGNAL_STRENGTH: typeName = "Signal Strength"; break;
                  case VALUE_MOISTURE: typeName = "Moisture"; break;
                  case VALUE_GENERIC: typeName = "Generic"; break;
                }
                Serial.printf("  %s: %.2f\n", typeName, received.values[i].value);
              }
              Serial.printf("Battery Percent: %d%%\n", received.header.batteryPercent);
              Serial.printf("RSSI: %d dBm\n", rssi);
              Serial.println("====================\n");
              
              if (received.header.batteryPercent > 80) {
                blinkLED(getColorGreen(), 1, 200);
              } else if (received.header.batteryPercent > 50) {
                blinkLED(getColorYellow(), 1, 200);
              } else if (received.header.batteryPercent > 20) {
                blinkLED(getColorOrange(), 2, 200);
              } else {
                blinkLED(getColorRed(), 3, 200);
              }
              setLED(getColorGreen());
            }
          }
        }
        
        lora_idle = true;
        return;
      }
    }
    
    // Check if it's an encrypted packet first
    if (size >= sizeof(EncryptedPacket) && payload[0] == 0xE0) {
      Serial.println("🔐 Encrypted packet detected, decrypting...");
      
      EncryptedPacket* encPkt = (EncryptedPacket*)payload;
      uint8_t decrypted[256];
      uint16_t decLen = securityManager.decryptPacket(encPkt, decrypted, sizeof(decrypted));
      
      if (decLen > 0) {
        Serial.printf("✅ Decryption successful! (%d bytes)\n", decLen);
        
        // Check if decrypted data is legacy SensorData
        if (decLen == sizeof(SensorData)) {
          SensorData received;
          memcpy(&received, decrypted, sizeof(SensorData));
          
          // Validate decrypted packet
          if (received.syncWord == SYNC_WORD && validateChecksum(&received)) {
            Serial.println("\n=== ENCRYPTED LEGACY PACKET RECEIVED ===");
            updateSensorInfo(received, rssi, snr);
            
            // Publish to MQTT
            SensorInfo* sensor = getSensorInfo(received.sensorId);
            if (sensor != NULL) {
              mqttClient.publishSensorData(
                received.sensorId,
                sensor->location,
                received.temperature,
                received.batteryPercent,
                rssi,
                snr
              );
              
              static bool discoveryPublished[256] = {false};
              if (!discoveryPublished[received.sensorId]) {
                mqttClient.publishHomeAssistantDiscovery(received.sensorId, sensor->location);
                discoveryPublished[received.sensorId] = true;
              }
            }
            
            if (wifiPortal.isDashboardActive()) {
              pendingWebSocketBroadcast = true;
            }
            
            Serial.printf("Sensor ID: %d\n", received.sensorId);
            Serial.printf("Temperature: %.2f°C\n", received.temperature);
            Serial.printf("Battery Voltage: %.2fV\n", received.batteryVoltage);
            Serial.printf("Battery Percent: %d%%\n", received.batteryPercent);
            Serial.printf("RSSI: %d dBm, SNR: %d dB\n", rssi, snr);
            Serial.println("====================\n");
            
            if (received.batteryPercent > 80) {
              blinkLED(getColorGreen(), 1, 200);
            } else if (received.batteryPercent > 50) {
              blinkLED(getColorYellow(), 1, 200);
            } else if (received.batteryPercent > 20) {
              blinkLED(getColorOrange(), 2, 200);
            } else {
              blinkLED(getColorRed(), 3, 200);
            }
            setLED(getColorGreen());
          } else {
            Serial.println("❌ Decrypted packet validation failed!");
          }
        }
      } else {
        Serial.println("❌ Decryption failed or packet rejected!");
      }
      
      Radio.Rx(0);
      lora_idle = true;
      return;
    }
    
    // Check if it's a legacy packet (unencrypted)
    if (size == sizeof(SensorData)) {
      SensorData received;
      memcpy(&received, payload, sizeof(SensorData));
      
      // Validate legacy packet (sync word, network ID, and checksum)
      if (received.syncWord == SYNC_WORD && 
          received.networkId == currentNetworkId && 
          validateChecksum(&received)) {
        Serial.println("\n=== LEGACY PACKET RECEIVED (UNENCRYPTED) ===");
        updateSensorInfo(received, rssi, snr);
        
        // Publish to MQTT
        SensorInfo* sensor = getSensorInfo(received.sensorId);
        if (sensor != NULL) {
          mqttClient.publishSensorData(
            received.sensorId,
            sensor->location,
            received.temperature,
            received.batteryPercent,
            rssi,
            snr
          );
          
          // Publish Home Assistant discovery on first packet
          static bool discoveryPublished[256] = {false};
          if (!discoveryPublished[received.sensorId]) {
            mqttClient.publishHomeAssistantDiscovery(received.sensorId, sensor->location);
            discoveryPublished[received.sensorId] = true;
          }
        }
        
        // Broadcast update to WebSocket clients for real-time dashboard updates
        if (wifiPortal.isDashboardActive()) {
          pendingWebSocketBroadcast = true;  // Set flag instead of calling from ISR
        }
        
        Serial.printf("Sensor ID: %d\n", received.sensorId);
        Serial.printf("Temperature: %.2f°C\n", received.temperature);
        Serial.printf("Battery Voltage: %.2fV\n", received.batteryVoltage);
        Serial.printf("Battery Percent: %d%%\n", received.batteryPercent);
        Serial.printf("Power State: %s\n", received.powerState ? "Charging" : "Discharging");
        Serial.printf("RSSI: %d dBm\n", rssi);
        Serial.printf("SNR: %d dB\n", snr);
        Serial.println("====================\n");
        
        // LED feedback based on battery level
        if (received.batteryPercent > 80) {
          blinkLED(getColorGreen(), 1, 200);
        } else if (received.batteryPercent > 50) {
          blinkLED(getColorYellow(), 1, 200);
        } else if (received.batteryPercent > 20) {
          blinkLED(getColorOrange(), 2, 200);
        } else {
          blinkLED(getColorRed(), 3, 200);
        }
        setLED(getColorGreen());
      } else {
        recordRxInvalid();
        Serial.println("Invalid legacy packet received");
        blinkLED(getColorRed(), 1, 100);
      }
    } 
    // Check if it's a multi-sensor packet
    else if (size >= sizeof(MultiSensorHeader) + sizeof(uint16_t)) {
      MultiSensorPacket received;
      
      // Parse header
      memcpy(&received.header, payload, sizeof(MultiSensorHeader));
      
      // Parse values (if any)
      size_t headerSize = sizeof(MultiSensorHeader);
      size_t valuesSize = received.header.valueCount * sizeof(SensorValuePacket);
      if (received.header.valueCount > 0 && received.header.valueCount <= MAX_VALUES_PER_PACKET) {
        memcpy(received.values, payload + headerSize, valuesSize);
      }
      
      // Read checksum from correct dynamic position
      uint16_t receivedChecksum;
      memcpy(&receivedChecksum, payload + headerSize + valuesSize, sizeof(uint16_t));
      
      Serial.printf("Checking multi-sensor packet: syncWord=0x%04X, type=%d, sensorId=%d, valueCount=%d\n",
                    received.header.syncWord, received.header.packetType, 
                    received.header.sensorId, received.header.valueCount);
      
      // Debug checksum validation
      uint16_t expectedChecksum = calculateMultiSensorChecksum(&received);
      Serial.printf("Checksum validation: received=0x%04X, expected=0x%04X, valid=%s\n",
                    receivedChecksum, expectedChecksum, 
                    (receivedChecksum == expectedChecksum) ? "YES" : "NO");
      
      // Validate multi-sensor packet
      if (received.header.syncWord == 0xABCD && 
          received.header.networkId == currentNetworkId && 
          received.header.packetType == PACKET_MULTI_SENSOR && 
          receivedChecksum == expectedChecksum) {
        Serial.println("\n=== MULTI-SENSOR PACKET RECEIVED ===");
        Serial.printf("Sensor ID: %d, Battery: %d%%, Values: %d\n",
                     received.header.sensorId, received.header.batteryPercent, 
                     received.header.valueCount);
        
        // Create temporary SensorData for updateSensorInfo (backward compatibility)
        SensorData legacyData;
        legacyData.syncWord = SYNC_WORD;
        legacyData.networkId = received.header.networkId;
        legacyData.sensorId = received.header.sensorId;
        legacyData.batteryVoltage = 0.0f;  // Not in multi-sensor header
        legacyData.batteryPercent = received.header.batteryPercent;
        legacyData.powerState = received.header.powerState;
        legacyData.temperature = -127.0f;  // Initialize to invalid/no reading
        // Copy location and zone from packet header
        strncpy(legacyData.location, received.header.location, sizeof(legacyData.location) - 1);
        legacyData.location[sizeof(legacyData.location) - 1] = '\0';
        strncpy(legacyData.zone, received.header.zone, sizeof(legacyData.zone) - 1);
        legacyData.zone[sizeof(legacyData.zone) - 1] = '\0';
        
        // Find temperature value for backward compatibility
        for (int i = 0; i < received.header.valueCount; i++) {
          if (received.values[i].type == VALUE_TEMPERATURE) {
            legacyData.temperature = received.values[i].value;
            break;
          }
        }
        
        updateSensorInfo(legacyData, rssi, snr);
        
        // Store individual sensor readings in PhysicalSensor structures
        for (int i = 0; i < received.header.valueCount; i++) {
          updateSensorReading(received.header.sensorId, i, 
                            received.values[i].type, received.values[i].value);
        }
        
        // Publish sensor data to MQTT
        SensorInfo* sensor = getSensorInfo(received.header.sensorId);
        if (sensor != NULL) {
          // Use new multi-sensor MQTT publish function
          mqttClient.publishMultiSensorData(
            received.header.sensorId,
            sensor->location,
            received.values,
            received.header.valueCount,
            received.header.batteryPercent,
            rssi,
            snr
          );
          
          // Publish Home Assistant discovery on first packet
          static bool discoveryPublished[256] = {false};
          if (!discoveryPublished[received.header.sensorId]) {
            mqttClient.publishHomeAssistantMultiSensorDiscovery(
              received.header.sensorId, 
              sensor->location,
              received.values,
              received.header.valueCount
            );
            discoveryPublished[received.header.sensorId] = true;
          }
        }
        
        // Broadcast update to WebSocket clients
        if (wifiPortal.isDashboardActive()) {
          pendingWebSocketBroadcast = true;  // Set flag instead of calling from ISR
        }
        
        #ifdef BASE_STATION
        // Check if this multi-sensor telemetry packet contains an ACK for a previous command
        extern RemoteConfigManager remoteConfigManager;
        if (received.header.lastCommandSeq != 0) {
          Serial.printf("ACK received from sensor %d (seq %d, status %d)\n",
                       received.header.sensorId, received.header.lastCommandSeq, 
                       received.header.ackStatus);
          
          // Clear the command from queue
          remoteConfigManager.handleAck(received.header.sensorId, received.header.lastCommandSeq, 
                                       received.header.ackStatus);
          
          // Diagnostics hook: record observed ACK with link stats
          wifiPortal.diagnosticsRecordAck(received.header.sensorId, received.header.lastCommandSeq, rssi, snr);
          
          if (received.header.ackStatus == 0) {
            Serial.println("✅ Command executed successfully!");
            
            // Check if this is a LoRa settings ACK and update tracking
            extern void updateLoRaRebootTracking(uint8_t sensorId);
            updateLoRaRebootTracking(received.header.sensorId);
          } else {
            Serial.printf("❌ Command failed with error code: %d\n", received.header.ackStatus);
          }
        }
        
        Serial.printf("DEBUG: Telemetry received from sensor %d\n", received.header.sensorId);
        
        // Schedule any pending commands shortly after RX completes.
        // Do NOT transmit from inside this callback.
        if (remoteConfigManager.getQueuedCount(received.header.sensorId) > 0) {
          if (!pendingCommandSend) {
            pendingCommandSend = true;
            pendingCommandSensorId = received.header.sensorId;
            pendingCommandReadyAtMs = millis() + BASE_RX_TO_TX_HOLDDOWN_MS;
            Serial.printf("📬 Sensor %d has pending commands; scheduling send in %lu ms...\n",
                         pendingCommandSensorId, (unsigned long)BASE_RX_TO_TX_HOLDDOWN_MS);
          } else {
            Serial.printf("📬 Pending command already scheduled; skipping schedule for sensor %d\n",
                         received.header.sensorId);
          }
        }
        #endif
        
        Serial.printf("Sensor ID: %d\n", received.header.sensorId);
        Serial.printf("Value Count: %d\n", received.header.valueCount);
        for (int i = 0; i < received.header.valueCount; i++) {
          const char* typeName = "Unknown";
          switch(received.values[i].type) {
            case VALUE_TEMPERATURE: typeName = "Temperature"; break;
            case VALUE_HUMIDITY: typeName = "Humidity"; break;
            case VALUE_PRESSURE: typeName = "Pressure"; break;
            case VALUE_LIGHT: typeName = "Light"; break;
            case VALUE_VOLTAGE: typeName = "Voltage"; break;
            case VALUE_CURRENT: typeName = "Current"; break;
            case VALUE_POWER: typeName = "Power"; break;
            case VALUE_ENERGY: typeName = "Energy"; break;
            case VALUE_GAS_RESISTANCE: typeName = "Gas Resistance"; break;
            case VALUE_BATTERY: typeName = "Battery"; break;
            case VALUE_SIGNAL_STRENGTH: typeName = "Signal Strength"; break;
            case VALUE_MOISTURE: typeName = "Moisture"; break;
            case VALUE_GENERIC: typeName = "Generic"; break;
          }
          Serial.printf("  %s: %.2f\n", typeName, received.values[i].value);
        }
        Serial.printf("Battery Percent: %d%%\n", received.header.batteryPercent);
        Serial.printf("Power State: %s\n", received.header.powerState ? "Charging" : "Discharging");
        Serial.printf("RSSI: %d dBm\n", rssi);
        Serial.printf("SNR: %d dB\n", snr);
        Serial.println("====================\n");
        
        // LED feedback based on battery level
        if (received.header.batteryPercent > 80) {
          blinkLED(getColorGreen(), 1, 200);
        } else if (received.header.batteryPercent > 50) {
          blinkLED(getColorYellow(), 1, 200);
        } else if (received.header.batteryPercent > 20) {
          blinkLED(getColorOrange(), 2, 200);
        } else {
          blinkLED(getColorRed(), 3, 200);
        }
        setLED(getColorGreen());
      } else {
        recordRxInvalid();
        Serial.println("Invalid multi-sensor packet received");
        blinkLED(getColorRed(), 1, 100);
      }
    } 
    else {
      recordRxInvalid();
      Serial.printf("Received packet with unexpected size: %d bytes\n", size);
    }
    
    lora_idle = true;
  #elif defined(SENSOR_NODE)
    // Sensor node - check for command packets from base station
    Serial.printf("RX: Received %d bytes\n", size);
    
    // Check if it's a command packet (syncWord = 0xCDEF)
    if (size >= sizeof(CommandPacket)) {
      CommandPacket* cmd = (CommandPacket*)payload;
      
      if (cmd->syncWord == COMMAND_SYNC_WORD) {
        Serial.printf("Command received: type=%d, target=%d, seq=%d\n", cmd->commandType, cmd->targetSensorId, cmd->sequenceNumber);
        
        // Check if command is for this sensor
        SensorConfig sensorConfig = configStorage.getSensorConfig();
        const bool isBroadcast = (cmd->targetSensorId == 0xFF);
        if (!isBroadcast && cmd->targetSensorId != sensorConfig.sensorId) {
          Serial.printf("Command not for this sensor (target=%d, my_id=%d) - ignoring\n", cmd->targetSensorId, sensorConfig.sensorId);
          lora_idle = true;
          return;
        }
        
        // Validate checksum using the remote_config checksum function
        extern RemoteConfigManager remoteConfigManager;
        size_t checksumLength = size - sizeof(uint16_t);
        uint16_t expectedChecksum = remoteConfigManager.calculateChecksum(payload, checksumLength);
        
        if (cmd->checksum == expectedChecksum) {
          // Process command and save ACK status for next telemetry packet
          bool success = false;
          
          // Show visual notification on OLED
          extern void showCommandNotification();
          showCommandNotification();

          // Happy beep for received commands
          buzzerPlayCommandReceived();

          // Special case: broadcast ping is used as a "wake screens" signal.
          // Do not force telemetry/ACKs to avoid multi-sensor collisions.
          if (isBroadcast && cmd->commandType == CMD_PING) {
            Serial.println("Broadcast wake ping received - waking display only (no ACK)");
            blinkLED(getColorBlue(), 1, 80);
            Radio.Rx(0);
            lora_idle = true;
            return;
          }
          
          switch (cmd->commandType) {
            case CMD_PING: {
              // Simple ping: ACK via immediate telemetry
              Serial.println("Ping command received - responding with ACK telemetry");
              success = true;
              break;
            }
            
            case CMD_BASE_WELCOME: {
              // Base station welcome packet with time sync
              Serial.println("Welcome packet received from base station!");
              if (cmd->dataLength >= 6) {
                uint32_t epochSec;
                int16_t tzOffsetMin;
                memcpy(&epochSec, &cmd->data[0], sizeof(uint32_t));
                memcpy(&tzOffsetMin, &cmd->data[4], sizeof(int16_t));
                Serial.printf("Time sync from welcome: epoch=%lu, tzOffset=%d min\n", 
                             (unsigned long)epochSec, (int)tzOffsetMin);
                
                // Apply system time
                struct timeval tv;
                tv.tv_sec = epochSec;
                tv.tv_usec = 0;
                settimeofday(&tv, NULL);
                #ifdef SENSOR_NODE
                setSensorLastTimeSyncEpoch(epochSec);
                #endif
                
                success = true;
                Serial.println("System time updated from base station welcome!");
                
                // Visual feedback
                extern void blinkLED(uint32_t color, int times, int delayMs);
                extern uint32_t getColorGreen();
                blinkLED(getColorGreen(), 3, 200);
              } else {
                Serial.println("Welcome packet has no time sync data");
                success = true;  // Still acknowledge the welcome
              }
              break;
            }
            
            case CMD_TIME_SYNC: {
              if (cmd->dataLength >= 6) {
                uint32_t epochSec;
                int16_t tzOffsetMin;
                memcpy(&epochSec, &cmd->data[0], sizeof(uint32_t));
                memcpy(&tzOffsetMin, &cmd->data[4], sizeof(int16_t));
                Serial.printf("Time sync received: epoch=%lu, tzOffset=%d min\n", (unsigned long)epochSec, (int)tzOffsetMin);
                
                // Apply timezone offset to convert UTC to local time
                time_t localTime = epochSec + (tzOffsetMin * 60);
                
                // Apply system time
                struct timeval tv;
                tv.tv_sec = localTime;
                tv.tv_usec = 0;
                settimeofday(&tv, NULL);
                #ifdef SENSOR_NODE
                setSensorLastTimeSyncEpoch(localTime);
                #endif
                
                success = true;
                Serial.printf("System time updated via LoRa time sync (local=%lu)\n", (unsigned long)localTime);
              }
              break;
            }
            case CMD_SET_INTERVAL: {
              if (cmd->dataLength == 2) {
                uint16_t interval = cmd->data[0] | (cmd->data[1] << 8);
                Serial.printf("Setting interval to %d seconds\n", interval);
                
                // Save to config
                SensorConfig config = configStorage.getSensorConfig();
                config.transmitInterval = interval;
                configStorage.setSensorConfig(config);
                
                success = true;
                Serial.println("Interval updated!");
              }
              break;
            }
            
            case CMD_SET_LOCATION: {
              if (cmd->dataLength > 0) {
                String location = String((char*)cmd->data);
                Serial.printf("Setting location to: %s\n", location.c_str());
                
                SensorConfig config = configStorage.getSensorConfig();
                strncpy(config.location, location.c_str(), sizeof(config.location) - 1);
                config.location[sizeof(config.location) - 1] = '\0';
                configStorage.setSensorConfig(config);
                
                success = true;
                Serial.println("Location updated!");
              }
              break;
            }
            
            case CMD_SET_TEMP_THRESH: {
              if (cmd->dataLength == 8) {
                float minTemp, maxTemp;
                memcpy(&minTemp, &cmd->data[0], sizeof(float));
                memcpy(&maxTemp, &cmd->data[4], sizeof(float));
                Serial.printf("Setting temp thresholds: %.1f to %.1f\n", minTemp, maxTemp);
                
                success = true;
                Serial.println("Temperature thresholds updated!");
              }
              break;
            }
            
            case CMD_RESTART: {
              Serial.println("Restart command received - restarting in 1 second!");
              success = true;
              // Save ACK info
              lastProcessedCommandSeq = cmd->sequenceNumber;
              lastCommandAckStatus = 0;
              // Delay then restart (sensor won't send telemetry, but that's OK)
              delay(1000);
              ESP.restart();
              break;
            }
            
            case CMD_SET_LORA_PARAMS: {
              if (cmd->dataLength >= 11) {
                uint32_t frequency;
                uint8_t spreadingFactor;
                uint32_t bandwidth;
                uint8_t txPower;
                uint8_t codingRate;
                
                // Unpack parameters from data buffer
                memcpy(&frequency, &cmd->data[0], sizeof(uint32_t));
                spreadingFactor = cmd->data[4];
                memcpy(&bandwidth, &cmd->data[5], sizeof(uint32_t));
                txPower = cmd->data[9];
                codingRate = cmd->data[10];
                
                Serial.println("===== LoRa Parameters Update =====");
                Serial.printf("Frequency: %u Hz\n", frequency);
                Serial.printf("Spreading Factor: SF%d\n", spreadingFactor);
                Serial.printf("Bandwidth: %u Hz\n", bandwidth);
                Serial.printf("TX Power: %d dBm\n", txPower);
                Serial.printf("Coding Rate: %d\n", codingRate);
                Serial.println("==================================");
                
                // Save to NVS for application after reboot
                Preferences prefs;
                prefs.begin("lora_params", false);
                prefs.putUInt("frequency", frequency);
                prefs.putUChar("sf", spreadingFactor);
                prefs.putUInt("bandwidth", bandwidth);
                prefs.putUChar("tx_power", txPower);
                prefs.putUChar("coding_rate", codingRate);
                prefs.putBool("pending", true);  // Flag that new params are waiting
                prefs.end();
                
                success = true;
                Serial.println("LoRa parameters saved! Will apply on next reboot.");
                Serial.println("⚠️ IMPORTANT: Base station must also reboot with matching parameters!");
                
                // Schedule automatic reboot after sending ACK (5 second delay)
                Serial.println("🔄 Scheduling automatic reboot in 5 seconds...");
                // This will be handled in main loop by checking a reboot flag
                extern bool loraRebootPending;
                extern uint32_t loraRebootTime;
                loraRebootPending = true;
                loraRebootTime = millis() + 5000;  // 5 seconds from now
              }
              break;
            }
            
            case CMD_GET_CONFIG: {
              Serial.println("Get config command received");
              // For GET commands, we'd need to include response data in next telemetry
              // For now just ACK it
              success = true;
              Serial.println("Config request acknowledged");
              break;
            }
            
            default:
              Serial.printf("Unknown command type: %d\n", cmd->commandType);
              success = false;
              break;
          }
          
          // Save ACK status for next telemetry transmission
          lastProcessedCommandSeq = cmd->sequenceNumber;
          lastCommandAckStatus = success ? 0 : 2; // 0 = success, 2 = execution failed
          
          // Activate forced interval mode briefly so the base has multiple chances to see the ACK
          forcedIntervalUntil = millis() + FORCED_INTERVAL_DURATION;
          ackFieldsValidUntil = forcedIntervalUntil;
          Serial.printf("🕐 Forced 10s interval activated for next 30 seconds (until %lu)\n", forcedIntervalUntil);
          
          Serial.printf("Command processed. ACK will be sent in next telemetry (seq %d, status %d)\n", 
                       lastProcessedCommandSeq, lastCommandAckStatus);
          
          blinkLED(success ? getColorGreen() : getColorRed(), 2, 100);
          
          // Trigger immediate telemetry send with ACK
          pendingAckSend = true;
          Serial.println("Command processed - will send immediate ACK telemetry");
          Radio.Rx(0);
          lora_idle = true;
        } else {
          Serial.println("Command checksum failed!");
          // Save NACK status
          lastProcessedCommandSeq = cmd->sequenceNumber;
          lastCommandAckStatus = 3; // Checksum error
          
          // Activate forced interval mode even on checksum error
          forcedIntervalUntil = millis() + FORCED_INTERVAL_DURATION;
          ackFieldsValidUntil = forcedIntervalUntil;
          Serial.printf("🕐 Forced 10s interval activated for next 30 seconds (until %lu)\n", forcedIntervalUntil);
          
          blinkLED(getColorRed(), 3, 50);
          // Trigger immediate telemetry send with NACK
          pendingAckSend = true;
          Serial.println("Checksum failed - will send immediate NACK telemetry");
          Radio.Rx(0);
          lora_idle = true;
        }
      } else {
        // Not a command for us, stay in RX mode
        LOGD("RX", "Not our command, continuing to listen");
        Radio.Rx(0);
        lora_idle = true;
      }
    } else {
      // Invalid command packet, stay in RX mode
      LOGW("RX", "Invalid command packet, continuing to listen");
      Radio.Rx(0);
      lora_idle = true;
    }
    
    // Only sleep/cleanup if we're not sending an ACK
    // (OnTxDone will handle cleanup after ACK transmission)
  #endif
}

void OnTxTimeout() {
  Serial.println("TX Timeout - Transmission failed!");
  recordTxFailure();
  lora_idle = true;
  #ifdef SENSOR_NODE
    blinkLED(getColorRed(), 2, 100);
    Radio.Sleep();
  #endif
}

void OnRxTimeout() {
  #ifdef BASE_STATION
    lora_idle = true;
  #endif
  #ifdef SENSOR_NODE
    // Stay in RX mode - sensor should always be listening for commands
    LOGD("RX", "RX timeout - continuing to listen");
    Radio.Rx(0);
    lora_idle = true;
  #endif
}

void OnRxError() {
  Serial.println("RX Error");
  #ifdef BASE_STATION
    blinkLED(getColorRed(), 1, 50);
    lora_idle = true;
  #endif
  #ifdef SENSOR_NODE
    Serial.println("RX Error on sensor - restarting RX");
    Radio.Rx(0);
    lora_idle = true;
  #endif
}

#ifdef BASE_STATION
// Handle pending WebSocket broadcast from main loop (not ISR)
void handlePendingWebSocketBroadcast() {
  extern WiFiPortal wifiPortal;
  
  if (pendingWebSocketBroadcast) {
    pendingWebSocketBroadcast = false;
    if (wifiPortal.isDashboardActive()) {
      wifiPortal.broadcastSensorUpdate();
    }
  }
}

// Send scheduled command after RX hold-down (main loop only)
void handlePendingCommandSend() {
  if (!pendingCommandSend) {
    return;
  }
  // Wait for hold-down time
  if ((int32_t)(millis() - pendingCommandReadyAtMs) < 0) {
    return;
  }

  uint8_t sensorId = pendingCommandSensorId;
  pendingCommandSend = false;
  pendingCommandSensorId = 0;
  pendingCommandReadyAtMs = 0;

  sendCommandNow(sensorId);
}

// Send command immediately to sensor (sensor is always listening)
void sendCommandNow(uint8_t sensorId) {
  extern RemoteConfigManager remoteConfigManager;
  
  CommandPacket* cmd = remoteConfigManager.getPendingCommand(sensorId);
  
  if (cmd == nullptr) {
    return;  // No command or still waiting for ACK
  }
  
  Serial.printf("🚀 Sending command type %d to sensor %d (seq %d, retry %d, targetSensorId=%d)\n", 
               cmd->commandType, sensorId, cmd->sequenceNumber,
               remoteConfigManager.getRetryCount(sensorId), cmd->targetSensorId);
  
  // CRITICAL DEBUG: Verify targetSensorId matches the queue ID
  if (cmd->targetSensorId != sensorId) {
    Serial.printf("⚠️ WARNING: Mismatch! Queue sensorId=%d but packet targetSensorId=%d\n", 
                 sensorId, cmd->targetSensorId);
  }
  
  size_t cmdSize = sizeof(CommandPacket);
  
  // Put radio in Standby if it's in RX mode
  Radio.Standby();
  
  // Diagnostics hook: record command send for link testing
  wifiPortal.diagnosticsRecordSent(sensorId, cmd->sequenceNumber);
  
  Radio.Send((uint8_t*)cmd, cmdSize);
  lora_idle = false;
}

// Check all sensors for commands that need retry
void checkCommandRetries() {
  extern RemoteConfigManager remoteConfigManager;

  // Only update retry state (timeouts). Actual sends happen after we receive telemetry
  // from a sensor and schedule a send during its RX window.
  remoteConfigManager.processRetries();

  // Reliability kick: if a command has been queued but we haven't managed to schedule a send
  // (e.g., missed RX scheduling), periodically schedule one attempt when the radio is idle.
  static uint32_t lastKickMs = 0;
  if (!pendingCommandSend && isLoRaIdle() && (millis() - lastKickMs) > 1000) {
    extern ClientInfo* getAllClients();
    ClientInfo* allClients = getAllClients();
    for (uint8_t i = 0; i < 10; i++) {
      const uint8_t clientId = allClients[i].clientId;
      if (clientId == 0) {
        continue;
      }
      if (remoteConfigManager.getQueuedCount(clientId) == 0) {
        continue;
      }

      uint8_t cmdType, seqNum, retries;
      bool waitingAck;
      uint32_t ageMs;
      if (remoteConfigManager.getCommandInfo(clientId, cmdType, seqNum, retries, waitingAck, ageMs) && !waitingAck) {
        pendingCommandSend = true;
        pendingCommandSensorId = clientId;
        pendingCommandReadyAtMs = millis();
        lastKickMs = millis();
        Serial.printf("📬 Kick-scheduling pending command send for sensor %d\n", pendingCommandSensorId);
        break;
      }
    }
  }
}
#endif

#ifdef SENSOR_NODE
// Check if we need to send immediate ACK telemetry
bool shouldSendImmediateAck() {
  if (pendingAckSend) {
    pendingAckSend = false;
    return true;
  }
  return false;
}

// Get the effective transmit interval (may be forced to 10s after command reception)
uint32_t getEffectiveTransmitInterval(uint32_t configuredInterval) {
  // Clear ACK fields and forced mode once the window expires
  if (ackFieldsValidUntil != 0 && (int32_t)(millis() - ackFieldsValidUntil) >= 0) {
    lastProcessedCommandSeq = 0;
    lastCommandAckStatus = 0;
    ackFieldsValidUntil = 0;
    forcedIntervalUntil = 0;
  }

  // If we're in forced interval mode and it hasn't expired
  if (millis() < forcedIntervalUntil) {
    // Use the shorter of: configured interval or forced 10s interval
    uint32_t forcedMs = FORCED_INTERVAL_MS;
    if (configuredInterval < forcedMs) {
      return configuredInterval;  // Use configured if it's already shorter than 10s
    } else {
      return forcedMs;  // Use 10s forced interval
    }
  }
  // Otherwise use configured interval normally
  return configuredInterval;
}
#endif
