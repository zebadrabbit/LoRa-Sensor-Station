#include "remote_config.h"
#include "logger.h"
#include <cstring>

static RemoteConfigManager* instance = nullptr;

void RemoteConfigManager::init() {
    if (mutex == nullptr) {
        mutex = xSemaphoreCreateMutex();
        if (mutex == nullptr) {
            LOGE("CMD", "RemoteConfigManager: failed to create mutex; concurrency issues may occur");
        }
    }

    nextSequenceNumber = 1;
    instance = this;
    
    // Clear failed command tracking
    memset(lastFailedCommand, 0, sizeof(lastFailedCommand));

    // Clear last command event tracking
    memset(lastSentCommand, 0, sizeof(lastSentCommand));
    memset(lastAckedCommand, 0, sizeof(lastAckedCommand));
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
    // Never block for long here: this can be called from web handlers.
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(25))) {
        return false;
    }

    if (dataLen > 248) {
        LOGE("CMD", "Command data too large: %d bytes", dataLen);
        if (mutex != nullptr) unlock();
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
    cmd.queuedAt = millis();
    cmd.lastAttempt = 0;
    cmd.timeout = COMMAND_TIMEOUT_MS;
    cmd.waitingForAck = false;
    
    commandQueues[sensorId].push(cmd);
    
    LOGI("CMD", "Queued command type %d for sensor %d (seq %d)", 
                  cmdType, sensorId, cmd.packet.sequenceNumber);
    
    // Don't send immediately - wait for next telemetry from sensor
    // Commands will be sent when sensor is in RX window (after telemetry)

    if (mutex != nullptr) unlock();
    return true;
}

bool RemoteConfigManager::getPendingCommand(uint8_t sensorId, CommandPacket& outPacket) {
    if (mutex != nullptr && !lock(portMAX_DELAY)) {
        return false;
    }

    if (commandQueues[sensorId].empty()) {
        if (mutex != nullptr) unlock();
        return false;
    }
    
    QueuedCommand& cmd = commandQueues[sensorId].front();
    
    // Check if we're already waiting for ACK
    if (cmd.waitingForAck) {
        // Check timeout
        if (millis() - cmd.lastAttempt > cmd.timeout) {
            LOGW("CMD", "Command timeout for sensor %d (seq %d), retry %d/%d", 
                         sensorId, cmd.packet.sequenceNumber, cmd.retryCount + 1, MAX_RETRY_COUNT);
            cmd.waitingForAck = false;
            cmd.retryCount++;
            
            if (cmd.retryCount >= MAX_RETRY_COUNT) {
                LOGE("CMD", "COMMAND FAILED: Max retries (%d) reached for sensor %d (seq %d)", 
                             MAX_RETRY_COUNT, sensorId, cmd.packet.sequenceNumber);
                LOGW("CMD", "Command dropped - sensor may be out of range or offline");
                
                // Track this failure
                lastFailedCommand[sensorId].commandType = cmd.packet.commandType;
                lastFailedCommand[sensorId].sequenceNumber = cmd.packet.sequenceNumber;
                lastFailedCommand[sensorId].failedAtMs = millis();
                lastFailedCommand[sensorId].reason = 0;  // timeout
                
                commandQueues[sensorId].pop();
                if (mutex != nullptr) unlock();
                return false;
            }
            
            LOGI("CMD", "Retrying command to sensor %d...", sensorId);
        } else {
            // Still waiting, don't send yet
            if (mutex != nullptr) unlock();
            return false;
        }
    }
    
    // Mark as sent and waiting for ACK
    cmd.lastAttempt = millis();
    cmd.waitingForAck = true;

    // Record last send attempt (includes retries)
    lastSentCommand[sensorId].commandType = cmd.packet.commandType;
    lastSentCommand[sensorId].sequenceNumber = cmd.packet.sequenceNumber;
    lastSentCommand[sensorId].statusCode = 0;
    lastSentCommand[sensorId].atMs = cmd.lastAttempt;

    // Copy packet out so callers don't hold a pointer into the queue.
    outPacket = cmd.packet;
    if (mutex != nullptr) unlock();
    return true;
}

void RemoteConfigManager::markCommandAcked(uint8_t sensorId, uint8_t sequenceNumber) {
    if (mutex != nullptr && !lock(portMAX_DELAY)) {
        return;
    }
    if (commandQueues[sensorId].empty()) {
        if (mutex != nullptr) unlock();
        return;
    }
    
    QueuedCommand& cmd = commandQueues[sensorId].front();
    if (cmd.packet.sequenceNumber == sequenceNumber) {
        LOGI("CMD", "Command ACKed for sensor %d (seq %d)", sensorId, sequenceNumber);

        // Record last ACK observed
        lastAckedCommand[sensorId].commandType = cmd.packet.commandType;
        lastAckedCommand[sensorId].sequenceNumber = sequenceNumber;
        lastAckedCommand[sensorId].statusCode = 0;
        lastAckedCommand[sensorId].atMs = millis();
        
        // Clear any failed command state on success
        lastFailedCommand[sensorId].failedAtMs = 0;
        
        // Record time-sync ACKs for display if applicable
        if (cmd.packet.commandType == CMD_TIME_SYNC) {
            extern void recordClientTimeSync(uint8_t clientId);
            recordClientTimeSync(sensorId);
            LOGD("CMD", "Recorded client %d time sync", sensorId);
        }
        commandQueues[sensorId].pop();
    }

    if (mutex != nullptr) unlock();
}

void RemoteConfigManager::markCommandFailed(uint8_t sensorId, uint8_t sequenceNumber, uint8_t statusCode) {
    if (mutex != nullptr && !lock(portMAX_DELAY)) {
        return;
    }
    if (commandQueues[sensorId].empty()) {
        if (mutex != nullptr) unlock();
        return;
    }
    
    QueuedCommand& cmd = commandQueues[sensorId].front();
    if (cmd.packet.sequenceNumber == sequenceNumber) {
        LOGW("CMD", "Command NACK/failure for sensor %d (seq %d, status=%d)", sensorId, sequenceNumber, statusCode);

        // Record last NACK/failure observed (only when we actually got a response)
        lastAckedCommand[sensorId].commandType = cmd.packet.commandType;
        lastAckedCommand[sensorId].sequenceNumber = sequenceNumber;
        lastAckedCommand[sensorId].statusCode = statusCode;
        lastAckedCommand[sensorId].atMs = millis();

        cmd.waitingForAck = false;
        cmd.retryCount++;
        
        if (cmd.retryCount >= MAX_RETRY_COUNT) {
            LOGW("CMD", "Max retries after NACK, dropping command");
            
            // Track this failure
            lastFailedCommand[sensorId].commandType = cmd.packet.commandType;
            lastFailedCommand[sensorId].sequenceNumber = cmd.packet.sequenceNumber;
            lastFailedCommand[sensorId].failedAtMs = millis();
            lastFailedCommand[sensorId].reason = 1;  // NACK
            
            commandQueues[sensorId].pop();
        }
    }

    if (mutex != nullptr) unlock();
}

void RemoteConfigManager::processAck(const AckPacket* ack) {
    // Validate checksum
    size_t checksumLength = sizeof(AckPacket) - sizeof(uint16_t);
    uint16_t expectedChecksum = calculateChecksum((const uint8_t*)ack, checksumLength);
    
    if (ack->checksum != expectedChecksum) {
        LOGW("CMD", "Invalid ACK checksum: received=0x%04X, expected=0x%04X",
                     ack->checksum, expectedChecksum);
        return;
    }
    
    if (ack->commandType == CMD_ACK) {
        markCommandAcked(ack->sensorId, ack->sequenceNumber);
        LOGI("CMD", "ACK from sensor %d: status=%d", ack->sensorId, ack->statusCode);
        
        // Process any response data
        if (ack->dataLength > 0) {
            String hex = "";
            for (int i = 0; i < ack->dataLength && i < 32; i++) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%02X ", ack->data[i]);
                hex += buf;
            }
            LOGD("CMD", "ACK data (%d bytes): %s", ack->dataLength, hex.c_str());
        }
    } else if (ack->commandType == CMD_NACK) {
        markCommandFailed(ack->sensorId, ack->sequenceNumber, ack->statusCode);
        LOGW("CMD", "NACK from sensor %d: error code=%d", ack->sensorId, ack->statusCode);
    }
}

void RemoteConfigManager::handleAck(uint8_t sensorId, uint8_t sequenceNumber, uint8_t status) {
    // Handle ACK embedded in telemetry packet
    if (status == 0) {
        // Success
        markCommandAcked(sensorId, sequenceNumber);
        LOGI("CMD", "Command executed successfully by sensor %d (seq %d)", sensorId, sequenceNumber);
    } else {
        // Error
        markCommandFailed(sensorId, sequenceNumber, status);
        LOGW("CMD", "Command failed on sensor %d (seq %d): error code=%d", sensorId, sequenceNumber, status);
    }
}

void RemoteConfigManager::processRetries() {
    if (mutex != nullptr && !lock(portMAX_DELAY)) {
        return;
    }
    // Check all queues for commands that need retry due to timeout.
    // NOTE: This is critical because getPendingCommand() is not called while we're waiting for ACK,
    // so timeouts must be advanced here.
    const uint32_t now = millis();
    for (int sensorId = 0; sensorId < 256; sensorId++) {
        if (commandQueues[sensorId].empty()) {
            continue;
        }

        QueuedCommand& cmd = commandQueues[sensorId].front();
        if (!cmd.waitingForAck) {
            continue;
        }

        if ((uint32_t)(now - cmd.lastAttempt) <= cmd.timeout) {
            continue;
        }

        LOGW("CMD", "Command timeout for sensor %d (seq %d), retry %d/%d",
             sensorId, cmd.packet.sequenceNumber, cmd.retryCount + 1, MAX_RETRY_COUNT);

        cmd.waitingForAck = false;
        cmd.retryCount++;

        if (cmd.retryCount >= MAX_RETRY_COUNT) {
            LOGE("CMD", "COMMAND FAILED: Max retries (%d) reached for sensor %d (seq %d)",
                 MAX_RETRY_COUNT, sensorId, cmd.packet.sequenceNumber);
            LOGW("CMD", "Command dropped - sensor may be out of range or offline");

            // Track this failure
            lastFailedCommand[sensorId].commandType = cmd.packet.commandType;
            lastFailedCommand[sensorId].sequenceNumber = cmd.packet.sequenceNumber;
            lastFailedCommand[sensorId].failedAtMs = now;
            lastFailedCommand[sensorId].reason = 0;  // timeout

            commandQueues[sensorId].pop();
        }
    }

    if (mutex != nullptr) unlock();
}

void RemoteConfigManager::clearCommands(uint8_t sensorId) {
    if (mutex != nullptr && !lock(portMAX_DELAY)) {
        return;
    }
    while (!commandQueues[sensorId].empty()) {
        commandQueues[sensorId].pop();
    }
    LOGI("CMD", "Cleared command queue for sensor %d", sensorId);

    if (mutex != nullptr) unlock();
}

uint8_t RemoteConfigManager::getQueuedCount(uint8_t sensorId) {
    // Don't ever block the webserver for long; return a safe default if busy.
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(5))) {
        return 0;
    }
    uint8_t count = commandQueues[sensorId].size();
    if (mutex != nullptr) unlock();
    return count;
}

uint8_t RemoteConfigManager::getRetryCount(uint8_t sensorId) {
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(5))) {
        return 0;
    }
    if (commandQueues[sensorId].empty()) {
        if (mutex != nullptr) unlock();
        return 0;
    }
    uint8_t retries = commandQueues[sensorId].front().retryCount;
    if (mutex != nullptr) unlock();
    return retries;
}

bool RemoteConfigManager::getCommandInfo(uint8_t sensorId, uint8_t& commandType, uint8_t& seqNum, 
                                         uint8_t& retries, bool& waitingAck, uint32_t& ageMs) {
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(5))) {
        return false;
    }
    if (commandQueues[sensorId].empty()) {
        if (mutex != nullptr) unlock();
        return false;
    }
    
    const QueuedCommand& cmd = commandQueues[sensorId].front();
    commandType = cmd.packet.commandType;
    seqNum = cmd.packet.sequenceNumber;
    retries = cmd.retryCount;
    waitingAck = cmd.waitingForAck;
    // If the command has never been sent, report how long it's been queued.
    // If it has been sent at least once, report age since the last send attempt.
    ageMs = (cmd.lastAttempt > 0)
              ? (millis() - cmd.lastAttempt)
              : (millis() - cmd.queuedAt);

    if (mutex != nullptr) unlock();
    return true;
}

bool RemoteConfigManager::getLastFailedCommand(uint8_t sensorId, uint8_t& commandType, 
                                               uint8_t& seqNum, uint32_t& ageMs, uint8_t& reason) {
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(5))) {
        return false;
    }
    // Check if there's a recorded failure (failedAtMs > 0 means it's valid)
    if (lastFailedCommand[sensorId].failedAtMs == 0) {
        if (mutex != nullptr) unlock();
        return false;
    }
    
    commandType = lastFailedCommand[sensorId].commandType;
    seqNum = lastFailedCommand[sensorId].sequenceNumber;
    ageMs = millis() - lastFailedCommand[sensorId].failedAtMs;
    reason = lastFailedCommand[sensorId].reason;

    if (mutex != nullptr) unlock();
    return true;
}

bool RemoteConfigManager::getLastSentCommand(uint8_t sensorId, uint8_t& commandType, uint8_t& seqNum, uint32_t& ageMs) {
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(5))) {
        return false;
    }
    if (lastSentCommand[sensorId].atMs == 0) {
        if (mutex != nullptr) unlock();
        return false;
    }
    commandType = lastSentCommand[sensorId].commandType;
    seqNum = lastSentCommand[sensorId].sequenceNumber;
    ageMs = millis() - lastSentCommand[sensorId].atMs;

    if (mutex != nullptr) unlock();
    return true;
}

bool RemoteConfigManager::getLastAckedCommand(uint8_t sensorId, uint8_t& commandType, uint8_t& seqNum, uint8_t& statusCode, uint32_t& ageMs) {
    if (mutex != nullptr && !lock(pdMS_TO_TICKS(5))) {
        return false;
    }
    if (lastAckedCommand[sensorId].atMs == 0) {
        if (mutex != nullptr) unlock();
        return false;
    }
    commandType = lastAckedCommand[sensorId].commandType;
    seqNum = lastAckedCommand[sensorId].sequenceNumber;
    statusCode = lastAckedCommand[sensorId].statusCode;
    ageMs = millis() - lastAckedCommand[sensorId].atMs;

    if (mutex != nullptr) unlock();
    return true;
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
    
    CommandPacket createTimeSync(uint8_t sensorId, uint32_t epochSeconds, int16_t tzOffsetMinutes) {
        CommandPacket cmd;
        cmd.syncWord = COMMAND_SYNC_WORD;
        cmd.commandType = CMD_TIME_SYNC;
        cmd.targetSensorId = sensorId;
        cmd.dataLength = 6;
        memcpy(&cmd.data[0], &epochSeconds, sizeof(uint32_t));
        memcpy(&cmd.data[4], &tzOffsetMinutes, sizeof(int16_t));
        return cmd;
    }
}
