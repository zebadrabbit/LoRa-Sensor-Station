#include "remote_config.h"
#include <cstring>

static RemoteConfigManager* instance = nullptr;

void RemoteConfigManager::init() {
    nextSequenceNumber = 1;
    instance = this;
}

uint16_t RemoteConfigManager::calculateChecksum(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool RemoteConfigManager::queueCommand(uint8_t sensorId, CommandType cmdType, const uint8_t* data, uint8_t dataLen) {
    if (dataLen > 248) {
        Serial.printf("Command data too large: %d bytes\n", dataLen);
        return false;
    }
    
    QueuedCommand cmd;
    cmd.packet.syncWord = COMMAND_SYNC_WORD;
    cmd.packet.commandType = cmdType;
    cmd.packet.targetSensorId = sensorId;
    cmd.packet.sequenceNumber = nextSequenceNumber++;
    cmd.packet.dataLength = dataLen;
    
    if (data != nullptr && dataLen > 0) {
        memcpy(cmd.packet.data, data, dataLen);
    }
    
    // Calculate checksum over header + data (excluding checksum field itself)
    size_t checksumLength = sizeof(CommandPacket) - sizeof(uint16_t);
    cmd.packet.checksum = calculateChecksum((const uint8_t*)&cmd.packet, checksumLength);
    
    cmd.retryCount = 0;
    cmd.lastAttempt = 0;
    cmd.timeout = COMMAND_TIMEOUT_MS;
    cmd.waitingForAck = false;
    
    commandQueues[sensorId].push(cmd);
    
    Serial.printf("Queued command type %d for sensor %d (seq %d)\n", 
                  cmdType, sensorId, cmd.packet.sequenceNumber);
    
    // Send immediately since sensor is always listening
    #ifdef BASE_STATION
    extern void sendCommandNow(uint8_t sensorId);
    sendCommandNow(sensorId);
    #endif
    
    return true;
}

CommandPacket* RemoteConfigManager::getPendingCommand(uint8_t sensorId) {
    if (commandQueues[sensorId].empty()) {
        return nullptr;
    }
    
    QueuedCommand& cmd = commandQueues[sensorId].front();
    
    // Check if we're already waiting for ACK
    if (cmd.waitingForAck) {
        // Check timeout
        if (millis() - cmd.lastAttempt > cmd.timeout) {
            Serial.printf("Command timeout for sensor %d (seq %d), retry %d/%d\n", 
                         sensorId, cmd.packet.sequenceNumber, cmd.retryCount + 1, MAX_RETRY_COUNT);
            cmd.waitingForAck = false;
            cmd.retryCount++;
            
            if (cmd.retryCount >= MAX_RETRY_COUNT) {
                Serial.printf("❌ COMMAND FAILED: Max retries (%d) reached for sensor %d (seq %d)\n", 
                             MAX_RETRY_COUNT, sensorId, cmd.packet.sequenceNumber);
                Serial.println("Command dropped - sensor may be out of range or offline");
                commandQueues[sensorId].pop();
                return nullptr;
            }
            
            Serial.printf("🔄 Retrying command to sensor %d...\n", sensorId);
        } else {
            // Still waiting, don't send yet
            return nullptr;
        }
    }
    
    // Mark as sent and waiting for ACK
    cmd.lastAttempt = millis();
    cmd.waitingForAck = true;
    
    return &cmd.packet;
}

void RemoteConfigManager::markCommandAcked(uint8_t sensorId, uint8_t sequenceNumber) {
    if (commandQueues[sensorId].empty()) {
        return;
    }
    
    QueuedCommand& cmd = commandQueues[sensorId].front();
    if (cmd.packet.sequenceNumber == sequenceNumber) {
        Serial.printf("Command ACKed for sensor %d (seq %d)\n", sensorId, sequenceNumber);
        commandQueues[sensorId].pop();
    }
}

void RemoteConfigManager::markCommandFailed(uint8_t sensorId, uint8_t sequenceNumber) {
    if (commandQueues[sensorId].empty()) {
        return;
    }
    
    QueuedCommand& cmd = commandQueues[sensorId].front();
    if (cmd.packet.sequenceNumber == sequenceNumber) {
        Serial.printf("Command NACK for sensor %d (seq %d)\n", sensorId, sequenceNumber);
        cmd.waitingForAck = false;
        cmd.retryCount++;
        
        if (cmd.retryCount >= MAX_RETRY_COUNT) {
            Serial.printf("Max retries after NACK, dropping command\n");
            commandQueues[sensorId].pop();
        }
    }
}

void RemoteConfigManager::processAck(const AckPacket* ack) {
    // Validate checksum
    size_t checksumLength = sizeof(AckPacket) - sizeof(uint16_t);
    uint16_t expectedChecksum = calculateChecksum((const uint8_t*)ack, checksumLength);
    
    if (ack->checksum != expectedChecksum) {
        Serial.printf("Invalid ACK checksum: received=0x%04X, expected=0x%04X\n",
                     ack->checksum, expectedChecksum);
        return;
    }
    
    if (ack->commandType == CMD_ACK) {
        markCommandAcked(ack->sensorId, ack->sequenceNumber);
        Serial.printf("ACK from sensor %d: status=%d\n", ack->sensorId, ack->statusCode);
        
        // Process any response data
        if (ack->dataLength > 0) {
            Serial.printf("ACK data (%d bytes): ", ack->dataLength);
            for (int i = 0; i < ack->dataLength && i < 32; i++) {
                Serial.printf("%02X ", ack->data[i]);
            }
            Serial.println();
        }
    } else if (ack->commandType == CMD_NACK) {
        markCommandFailed(ack->sensorId, ack->sequenceNumber);
        Serial.printf("NACK from sensor %d: error code=%d\n", ack->sensorId, ack->statusCode);
    }
}

void RemoteConfigManager::handleAck(uint8_t sensorId, uint8_t sequenceNumber, uint8_t status) {
    // Handle ACK embedded in telemetry packet
    if (status == 0) {
        // Success
        markCommandAcked(sensorId, sequenceNumber);
        Serial.printf("✅ Command executed successfully by sensor %d (seq %d)\n", sensorId, sequenceNumber);
    } else {
        // Error
        markCommandFailed(sensorId, sequenceNumber);
        Serial.printf("⚠️  Command failed on sensor %d (seq %d): error code=%d\n", sensorId, sequenceNumber, status);
    }
}

void RemoteConfigManager::processRetries() {
    // Check all queues for commands that need retry due to timeout
    for (int i = 0; i < 256; i++) {
        if (!commandQueues[i].empty()) {
            QueuedCommand& cmd = commandQueues[i].front();
            
            if (cmd.waitingForAck && (millis() - cmd.lastAttempt > cmd.timeout)) {
                Serial.printf("Retry timeout check for sensor %d\n", i);
                cmd.waitingForAck = false;
            }
        }
    }
}

void RemoteConfigManager::clearCommands(uint8_t sensorId) {
    while (!commandQueues[sensorId].empty()) {
        commandQueues[sensorId].pop();
    }
    Serial.printf("Cleared command queue for sensor %d\n", sensorId);
}

uint8_t RemoteConfigManager::getQueuedCount(uint8_t sensorId) {
    return commandQueues[sensorId].size();
}

uint8_t RemoteConfigManager::getRetryCount(uint8_t sensorId) {
    if (commandQueues[sensorId].empty()) {
        return 0;
    }
    return commandQueues[sensorId].front().retryCount;
}

// Command builder implementations
namespace CommandBuilder {
    CommandPacket createSetInterval(uint8_t sensorId, uint16_t intervalSeconds) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_SET_INTERVAL;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 2;
        cmd.data[0] = intervalSeconds & 0xFF;
        cmd.data[1] = (intervalSeconds >> 8) & 0xFF;
        return cmd;
    }
    
    CommandPacket createSetLocation(uint8_t sensorId, const char* location) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_SET_LOCATION;
        cmd.targetSensorId = sensorId;
        
        size_t len = strlen(location);
        if (len > 247) len = 247;
        
        cmd.dataLength = len + 1;
        strncpy((char*)cmd.data, location, len);
        cmd.data[len] = '\0';
        
        return cmd;
    }
    
    CommandPacket createSetTempThreshold(uint8_t sensorId, float minTemp, float maxTemp) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_SET_TEMP_THRESH;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 8;
        
        memcpy(&cmd.data[0], &minTemp, sizeof(float));
        memcpy(&cmd.data[4], &maxTemp, sizeof(float));
        
        return cmd;
    }
    
    CommandPacket createSetBatteryThreshold(uint8_t sensorId, uint8_t lowPercent, uint8_t criticalPercent) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_SET_BATTERY_THRESH;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 2;
        cmd.data[0] = lowPercent;
        cmd.data[1] = criticalPercent;
        return cmd;
    }
    
    CommandPacket createSetMeshConfig(uint8_t sensorId, bool enabled, bool forwarding) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_SET_MESH_CONFIG;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 2;
        cmd.data[0] = enabled ? 1 : 0;
        cmd.data[1] = forwarding ? 1 : 0;
        return cmd;
    }
    
    CommandPacket createGetConfig(uint8_t sensorId) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_GET_CONFIG;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 0;
        return cmd;
    }
    
    CommandPacket createRestart(uint8_t sensorId) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_RESTART;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 0;
        return cmd;
    }
    
    CommandPacket createSetLoRaParams(uint8_t sensorId, uint32_t frequency, 
                                     uint8_t spreadingFactor, uint32_t bandwidth,
                                     uint8_t txPower, uint8_t codingRate) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_SET_LORA_PARAMS;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 14;  // 4 + 1 + 4 + 1 + 1 + 3 (padding for alignment)
        
        // Pack parameters into data buffer
        // Frequency: 4 bytes (uint32_t)
        memcpy(&cmd.data[0], &frequency, sizeof(uint32_t));
        
        // Spreading Factor: 1 byte (uint8_t)
        cmd.data[4] = spreadingFactor;
        
        // Bandwidth: 4 bytes (uint32_t) - will be 125000, 250000, or 500000 Hz
        memcpy(&cmd.data[5], &bandwidth, sizeof(uint32_t));
        
        // TX Power: 1 byte (uint8_t)
        cmd.data[9] = txPower;
        
        // Coding Rate: 1 byte (uint8_t) - CR_4_5=1, CR_4_6=2, CR_4_7=3, CR_4_8=4
        cmd.data[10] = codingRate;
        
        return cmd;
    }
}
