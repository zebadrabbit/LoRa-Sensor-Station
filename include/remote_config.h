#ifndef REMOTE_CONFIG_H
#define REMOTE_CONFIG_H

#include <Arduino.h>
#include <queue>

// Command types for remote configuration
enum CommandType : uint8_t {
    CMD_PING = 0x00,              // Ping/keepalive
    CMD_GET_CONFIG = 0x01,        // Request current config
    CMD_SET_INTERVAL = 0x02,      // Set transmission interval
    CMD_SET_LOCATION = 0x03,      // Set sensor location/name
    CMD_SET_TEMP_THRESH = 0x04,   // Set temperature thresholds
    CMD_SET_BATTERY_THRESH = 0x05,// Set battery thresholds
    CMD_SET_MESH_CONFIG = 0x06,   // Enable/disable mesh
    CMD_RESTART = 0x07,           // Restart device
    CMD_FACTORY_RESET = 0x08,     // Factory reset
    CMD_ACK = 0xA0,               // Acknowledgment
    CMD_NACK = 0xA1               // Negative acknowledgment
};

// Command packet structure (max 200 bytes to fit within LoRa SF10 limit)
struct __attribute__((packed)) CommandPacket {
    uint16_t syncWord;            // 0xCDEF for command packets
    uint8_t commandType;          // CommandType enum
    uint8_t targetSensorId;       // Target sensor/client ID
    uint8_t sequenceNumber;       // For tracking ACKs
    uint8_t dataLength;           // Length of payload data
    uint8_t data[192];            // Command-specific payload (reduced to fit SF10 limit)
    uint16_t checksum;            // CRC16 checksum
};

// ACK/NACK response structure
struct __attribute__((packed)) AckPacket {
    uint16_t syncWord;            // 0xCDEF
    uint8_t commandType;          // CMD_ACK or CMD_NACK
    uint8_t sensorId;             // Responding sensor ID
    uint8_t sequenceNumber;       // Matches command sequence
    uint8_t statusCode;           // Success/error code
    uint8_t dataLength;           // Optional response data length
    uint8_t data[192];            // Optional response data (reduced to match CommandPacket)
    uint16_t checksum;
};

// Queued command with retry logic
struct QueuedCommand {
    CommandPacket packet;
    uint8_t retryCount;
    uint32_t lastAttempt;
    uint32_t timeout;
    bool waitingForAck;
};

#define COMMAND_SYNC_WORD 0xCDEF
#define MAX_RETRY_COUNT 3
#define COMMAND_TIMEOUT_MS 10000  // 10 seconds

// Command queue for each sensor
class RemoteConfigManager {
public:
    void init();
    
    // Queue a command to send to a sensor
    bool queueCommand(uint8_t sensorId, CommandType cmdType, const uint8_t* data, uint8_t dataLen);
    
    // Get next pending command for a sensor (returns NULL if none)
    CommandPacket* getPendingCommand(uint8_t sensorId);
    
    // Mark command as acknowledged
    void markCommandAcked(uint8_t sensorId, uint8_t sequenceNumber);
    
    // Mark command as failed (for retry)
    void markCommandFailed(uint8_t sensorId, uint8_t sequenceNumber);
    
    // Process ACK/NACK received from sensor (old method - for compatibility)
    void processAck(const AckPacket* ack);
    
    // Handle ACK embedded in telemetry packet (new piggyback method)
    void handleAck(uint8_t sensorId, uint8_t sequenceNumber, uint8_t status);
    
    // Check for commands that need retry
    void processRetries();
    
    // Clear all pending commands for a sensor
    void clearCommands(uint8_t sensorId);
    
    // Get command queue status
    uint8_t getQueuedCount(uint8_t sensorId);
    
    // Get retry count for current command
    uint8_t getRetryCount(uint8_t sensorId);
    
    // Checksum calculation (public for sensor use)
    uint16_t calculateChecksum(const uint8_t* data, size_t length);
    
private:
    std::queue<QueuedCommand> commandQueues[256]; // One queue per sensor ID
    uint8_t nextSequenceNumber;
};

// Helper functions for creating specific commands
namespace CommandBuilder {
    // Create SET_INTERVAL command (interval in seconds)
    CommandPacket createSetInterval(uint8_t sensorId, uint16_t intervalSeconds);
    
    // Create SET_LOCATION command
    CommandPacket createSetLocation(uint8_t sensorId, const char* location);
    
    // Create SET_TEMP_THRESH command
    CommandPacket createSetTempThreshold(uint8_t sensorId, float minTemp, float maxTemp);
    
    // Create SET_BATTERY_THRESH command
    CommandPacket createSetBatteryThreshold(uint8_t sensorId, uint8_t lowPercent, uint8_t criticalPercent);
    
    // Create SET_MESH_CONFIG command
    CommandPacket createSetMeshConfig(uint8_t sensorId, bool enabled, bool forwarding);
    
    // Create GET_CONFIG command
    CommandPacket createGetConfig(uint8_t sensorId);
    
    // Create RESTART command
    CommandPacket createRestart(uint8_t sensorId);
}

#endif // REMOTE_CONFIG_H
