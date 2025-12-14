#include "lora_comm.h"
#include "config.h"
#include "led_control.h"
#include "display_control.h"
#include "statistics.h"
#include "wifi_portal.h"
#include "sensor_interface.h"
#include "mesh_routing.h"
#include "config_storage.h"
#ifdef BASE_STATION
#include "mqtt_client.h"
#include "remote_config.h"
#endif
#ifdef SENSOR_NODE
#include "remote_config.h"
#endif
#include <Arduino.h>

static RadioEvents_t RadioEvents;
static bool lora_idle = true;
static uint16_t currentNetworkId = 0;  // Current network ID for validation

#ifdef BASE_STATION
static bool pendingWebSocketBroadcast = false;  // Flag to trigger broadcast from main loop
static bool pendingCommandSend = false;  // Flag to send command from main loop
static uint8_t pendingCommandSensorId = 0;
#endif

#ifdef SENSOR_NODE
uint8_t lastProcessedCommandSeq = 0;  // Last command sequence number we processed
uint8_t lastCommandAckStatus = 0;     // Status of last command (0=success, non-zero=error)
static bool pendingAckSend = false;   // Flag to send immediate telemetry with ACK
#endif

// Calculate LoRa sync word from network ID
// Maps network ID (1-65535) to valid sync word range (0x12-0xFF)
uint8_t calculateSyncWord(uint16_t networkId) {
    return 0x12 + (networkId % 244);  // 0x12 to 0xFF (244 values)
}

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
  
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  
  // Set hardware sync word for network isolation (SX1262 specific)
  // Note: SX1262 uses 2-byte sync word, we use the calculated byte twice
  Radio.SetSyncWord((syncWord << 8) | syncWord);
  
  Radio.SetTxConfig(
    MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH, false,  // Changed to variable length
    true, 0, 0, LORA_IQ_INVERSION_ON, 3000
  );
  
  Radio.SetRxConfig(
    MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT, false,  // Changed to variable length
    0, true, 0, 0, LORA_IQ_INVERSION_ON, true  // Max payload 0 = use variable length mode
  );
  
  #ifdef BASE_STATION
    Serial.println("Base station ready - listening for sensors...");
    Serial.printf("Frequency: %d Hz\n", RF_FREQUENCY);
    Serial.printf("Bandwidth: %d, SF: %d, CR: %d\n", LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODINGRATE);
    Serial.printf("Network ID: %d (Sync Word: 0x%02X)\n", currentNetworkId, syncWord);
    Serial.printf("Expected packet size: %d bytes\n", sizeof(SensorData));
  #elif defined(SENSOR_NODE)
    Serial.println("Sensor node ready - preparing to send data...");
    Serial.printf("Network ID: %d (Sync Word: 0x%02X)\n", currentNetworkId, syncWord);
  #endif
}

bool isLoRaIdle() {
  return lora_idle;
}

void setLoRaIdle(bool idle) {
  lora_idle = idle;
}

void sendSensorData(const SensorData& data) {
  Serial.println("Transmitting...");
  Serial.printf("Packet size: %d bytes\n", sizeof(SensorData));
  
  recordTxAttempt();
  
  Radio.Sleep();
  delay(10);
  Radio.Send((uint8_t*)&data, sizeof(SensorData));
  lora_idle = false;
}

void enterRxMode() {
  if (lora_idle) {
    lora_idle = false;
    Serial.println("Entering RX mode...");
    Radio.Rx(0);
  }
}

// ============================================================================
// RADIO CALLBACKS
// ============================================================================
void OnTxDone() {
  Serial.println("TX Done - Packet sent successfully!");
  recordTxSuccess();
  #ifdef SENSOR_NODE
    blinkLED(getColorBlue(), 2, 100);
    // Clear ACK fields after sending them once
    lastProcessedCommandSeq = 0;
    lastCommandAckStatus = 0;
    // Go back to RX mode to listen for commands
    // Use Standby to properly reset radio state after TX
    Radio.Standby();
    delay(100);
    Serial.println("Back to RX mode, listening for commands...");
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
    Serial.println("ERROR: OnRxDone called with null payload!");
    return;
  }
  
  // Ignore 0-byte packets (CRC errors or sync word mismatches)
  if (size == 0) {
    Serial.println("RX: Received 0 bytes (CRC error or sync mismatch) - ignoring");
    return;
  }
  
  #ifdef BASE_STATION
    Serial.printf("RX: Received %d bytes, RSSI: %d, SNR: %d\n", size, rssi, snr);
    Serial.printf("Expected legacy size: %d bytes\n", sizeof(SensorData));
  #elif defined(SENSOR_NODE)
    Serial.printf("RX: Received %d bytes, RSSI: %d, SNR: %d\n", size, rssi, snr);
    Serial.print("DEBUG: First 20 bytes: ");
    for (int i = 0; i < min(20, (int)size); i++) {
      Serial.printf("%02X ", payload[i]);
    }
    Serial.println();
  #endif
  
  #ifdef BASE_STATION
    Radio.Sleep();
    
    recordRxPacket(rssi);
    
    // Only check for mesh packets if mesh is enabled
    BaseStationConfig baseConfig = configStorage.getBaseStationConfig();
    Serial.printf("Mesh enabled: %s\n", baseConfig.meshEnabled ? "YES" : "NO");
    
    // Check if it's a mesh packet (first byte is MeshPacketType enum)
    if (baseConfig.meshEnabled && size >= sizeof(MeshHeader)) {
      MeshHeader* meshHdr = (MeshHeader*)payload;
      if (meshHdr->packetType >= MESH_DATA && meshHdr->packetType <= MESH_NEIGHBOR_BEACON) {
        Serial.println("Mesh packet detected, processing...");
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
              
              for (int i = 0; i < received.header.valueCount; i++) {
                if (received.values[i].type == VALUE_TEMPERATURE) {
                  legacyData.temperature = received.values[i].value;
                  break;
                }
              }
              
              updateSensorInfo(legacyData, rssi, snr);
              
              SensorInfo* sensor = getSensorInfo(received.header.sensorId);
              if (sensor != NULL) {
                for (int i = 0; i < received.header.valueCount; i++) {
                  if (received.values[i].type == VALUE_TEMPERATURE) {
                    mqttClient.publishSensorData(
                      received.header.sensorId,
                      sensor->location,
                      received.values[i].value,
                      received.header.batteryPercent,
                      rssi,
                      snr
                    );
                    break;
                  }
                }
                
                static bool discoveryPublished[256] = {false};
                if (!discoveryPublished[received.header.sensorId]) {
                  mqttClient.publishHomeAssistantDiscovery(received.header.sensorId, sensor->location);
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
    
    // Check if it's a legacy packet
    if (size == sizeof(SensorData)) {
      SensorData received;
      memcpy(&received, payload, sizeof(SensorData));
      
      // Validate legacy packet (sync word, network ID, and checksum)
      if (received.syncWord == SYNC_WORD && 
          received.networkId == currentNetworkId && 
          validateChecksum(&received)) {
        Serial.println("\n=== LEGACY PACKET RECEIVED ===");
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
        
        // Find temperature value for backward compatibility
        for (int i = 0; i < received.header.valueCount; i++) {
          if (received.values[i].type == VALUE_TEMPERATURE) {
            legacyData.temperature = received.values[i].value;
            break;
          }
        }
        
        updateSensorInfo(legacyData, rssi, snr);
        
        // Publish each sensor value to MQTT
        SensorInfo* sensor = getSensorInfo(received.header.sensorId);
        if (sensor != NULL) {
          // Publish temperature if available (for legacy compatibility)
          for (int i = 0; i < received.header.valueCount; i++) {
            if (received.values[i].type == VALUE_TEMPERATURE) {
              mqttClient.publishSensorData(
                received.header.sensorId,
                sensor->location,
                received.values[i].value,
                received.header.batteryPercent,
                rssi,
                snr
              );
              break;
            }
          }
          
          // Publish Home Assistant discovery on first packet
          static bool discoveryPublished[256] = {false};
          if (!discoveryPublished[received.header.sensorId]) {
            mqttClient.publishHomeAssistantDiscovery(received.header.sensorId, sensor->location);
            discoveryPublished[received.header.sensorId] = true;
          }
        }
        
        // Broadcast update to WebSocket clients
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
        Serial.printf("Power State: %s\n", received.header.powerState ? "Charging" : "Discharging");
        Serial.printf("RSSI: %d dBm\n", rssi);
        Serial.printf("SNR: %d dB\n", snr);
        Serial.println("====================\n");
        
        #ifdef BASE_STATION
        // Check if this telemetry packet contains an ACK for a previous command
        extern RemoteConfigManager remoteConfigManager;
        if (received.header.lastCommandSeq != 0) {
          Serial.printf("ACK received from sensor %d (seq %d, status %d)\n",
                       received.header.sensorId, received.header.lastCommandSeq, 
                       received.header.ackStatus);
          
          // Clear the command from queue
          remoteConfigManager.handleAck(received.header.sensorId, received.header.lastCommandSeq, 
                                       received.header.ackStatus);
          
          if (received.header.ackStatus == 0) {
            Serial.println("Command executed successfully!");
          } else {
            Serial.printf("Command failed with error code: %d\n", received.header.ackStatus);
          }
        }
        
        // Check if there are pending commands for this sensor
        // Commands are now sent immediately when queued, not here
        Serial.printf("DEBUG: Telemetry received from sensor %d\n", received.header.sensorId);
        #endif
        
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
        Serial.printf("Command received: type=%d, seq=%d\n", cmd->commandType, cmd->sequenceNumber);
        
        // Validate checksum using the remote_config checksum function
        extern RemoteConfigManager remoteConfigManager;
        size_t checksumLength = size - sizeof(uint16_t);
        uint16_t expectedChecksum = remoteConfigManager.calculateChecksum(payload, checksumLength);
        
        if (cmd->checksum == expectedChecksum) {
          // Process command and save ACK status for next telemetry packet
          bool success = false;
          
          switch (cmd->commandType) {
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
          blinkLED(getColorRed(), 3, 50);
          // Trigger immediate telemetry send with NACK
          pendingAckSend = true;
          Serial.println("Checksum failed - will send immediate NACK telemetry");
          Radio.Rx(0);
          lora_idle = true;
        }
      } else {
        // Not a command for us, go back to RX mode
        Serial.println("Not our command, back to RX");
        Radio.Rx(0);
        lora_idle = true;
      }
    } else {
      // Invalid command packet, go back to sleep
      Radio.Sleep();
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
    // No command received, go back to sleep
    Serial.println("RX timeout - no commands received");
    Radio.Sleep();
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

// Send command immediately to sensor (sensor is always listening)
void sendCommandNow(uint8_t sensorId) {
  extern RemoteConfigManager remoteConfigManager;
  
  CommandPacket* cmd = remoteConfigManager.getPendingCommand(sensorId);
  
  if (cmd == nullptr) {
    return;  // No command or still waiting for ACK
  }
  
  Serial.printf("Sending command type %d to sensor %d (seq %d, retry %d)\n", 
               cmd->commandType, cmd->targetSensorId, cmd->sequenceNumber,
               remoteConfigManager.getRetryCount(sensorId));
  
  size_t cmdSize = sizeof(CommandPacket);
  
  // Put radio in Standby if it's in RX mode
  Radio.Standby();
  
  Radio.Send((uint8_t*)cmd, cmdSize);
  lora_idle = false;
}

// Check all sensors for commands that need retry
void checkCommandRetries() {
  extern RemoteConfigManager remoteConfigManager;
  
  // Check sensors 1-10 (common sensor ID range)
  for (uint8_t sensorId = 1; sensorId <= 10; sensorId++) {
    if (remoteConfigManager.getQueuedCount(sensorId) > 0) {
      sendCommandNow(sensorId);
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
#endif
