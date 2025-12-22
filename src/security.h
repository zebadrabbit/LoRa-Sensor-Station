/**
 * @file security.h
 * @brief Network security module for LoRa communication
 * 
 * Provides AES-128 encryption and device whitelisting for secure LoRa networks.
 * Part of Network Pairing Phase 2.
 * 
 * Features:
 * - AES-128-CBC encryption for packet payloads
 * - Device whitelist with NVS persistence
 * - 128-bit encryption key management
 * - HMAC-based message authentication
 * - Replay attack prevention with sequence numbers
 * 
 * @version 2.13.0
 * @date December 14, 2025
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include "mbedtls/aes.h"
#include "mbedtls/md.h"

// Security configuration
#define LORA_AES_KEY_SIZE 16     // 128-bit key
#define LORA_AES_BLOCK_SIZE 16   // 128-bit blocks
#define MAX_WHITELIST_SIZE 32    // Maximum whitelisted devices
#define HMAC_SIZE 8              // Truncated HMAC for space efficiency

/**
 * @brief Security configuration stored in NVS
 */
struct SecurityConfig {
    bool encryptionEnabled;                      // Enable/disable encryption
    bool whitelistEnabled;                       // Enable/disable whitelist
    uint8_t encryptionKey[LORA_AES_KEY_SIZE];   // 128-bit AES key
    uint8_t whitelistCount;                      // Number of whitelisted devices
    uint8_t whitelist[MAX_WHITELIST_SIZE];       // Whitelisted sensor IDs
    uint32_t sequenceNumber;                     // Replay prevention counter
};

/**
 * @brief Encrypted packet structure
 * 
 * Format:
 * [1B packetType][1B sensorId][2B networkId][4B sequence][8B HMAC][NB encrypted payload][16B IV]
 */
struct EncryptedPacket {
    uint8_t packetType;
    uint8_t sensorId;
    uint16_t networkId;
    uint32_t sequenceNumber;
    uint8_t hmac[HMAC_SIZE];
    uint8_t payload[200];  // Maximum encrypted payload
    uint16_t payloadSize;
    uint8_t iv[LORA_AES_BLOCK_SIZE];  // Initialization vector
};

/**
 * @brief Security manager class
 */
class SecurityManager {
public:
    SecurityManager();
    
    /**
     * @brief Initialize security system from NVS
     */
    bool begin();
    
    /**
     * @brief Generate a new random AES encryption key
     */
    void generateKey();
    
    /**
     * @brief Set encryption key manually (for network pairing)
     * @param key 16-byte AES key
     */
    void setKey(const uint8_t* key);
    
    /**
     * @brief Get current encryption key
     * @param key Buffer to receive 16-byte key
     */
    void getKey(uint8_t* key);
    
    /**
     * @brief Encrypt packet payload
     * @param plaintext Raw packet data
     * @param length Plaintext length
     * @param encrypted Output buffer for encrypted packet
     * @param sensorId Sender ID
     * @param networkId Network ID
     * @return Encrypted packet size, or 0 on failure
     */
    uint16_t encryptPacket(const uint8_t* plaintext, uint16_t length, 
                           EncryptedPacket* encrypted, uint8_t sensorId, uint16_t networkId);
    
    /**
     * @brief Decrypt packet payload
     * @param encrypted Encrypted packet
     * @param plaintext Output buffer for decrypted data
     * @param maxLength Maximum plaintext buffer size
     * @return Decrypted length, or 0 on failure/authentication error
     */
    uint16_t decryptPacket(const EncryptedPacket* encrypted, uint8_t* plaintext, uint16_t maxLength);
    
    /**
     * @brief Add device to whitelist
     * @param sensorId Sensor ID to whitelist
     * @return true if added successfully
     */
    bool addToWhitelist(uint8_t sensorId);
    
    /**
     * @brief Remove device from whitelist
     * @param sensorId Sensor ID to remove
     * @return true if removed successfully
     */
    bool removeFromWhitelist(uint8_t sensorId);
    
    /**
     * @brief Check if device is whitelisted
     * @param sensorId Sensor ID to check
     * @return true if device is on whitelist
     */
    bool isWhitelisted(uint8_t sensorId);
    
    /**
     * @brief Clear entire whitelist
     */
    void clearWhitelist();
    
    /**
     * @brief Get whitelist as array
     * @param list Output buffer for whitelist
     * @param maxSize Maximum list size
     * @return Number of entries copied
     */
    uint8_t getWhitelist(uint8_t* list, uint8_t maxSize);
    
    /**
     * @brief Enable/disable encryption
     */
    void setEncryptionEnabled(bool enabled);
    
    /**
     * @brief Enable/disable whitelist
     */
    void setWhitelistEnabled(bool enabled);
    
    /**
     * @brief Check if encryption is enabled
     */
    bool isEncryptionEnabled() const { return config.encryptionEnabled; }
    
    /**
     * @brief Check if whitelist is enabled
     */
    bool isWhitelistEnabled() const { return config.whitelistEnabled; }
    
    /**
     * @brief Save configuration to NVS
     */
    bool saveConfig();
    
    /**
     * @brief Load configuration from NVS
     */
    bool loadConfig();
    
    /**
     * @brief Get current configuration
     */
    SecurityConfig getConfig() const { return config; }

private:
    SecurityConfig config;
    
    /**
     * @brief Generate random IV for encryption
     */
    void generateIV(uint8_t* iv);
    
    /**
     * @brief Calculate HMAC for authentication
     */
    void calculateHMAC(const uint8_t* data, uint16_t length, uint8_t* hmac);
    
    /**
     * @brief Verify HMAC
     */
    bool verifyHMAC(const uint8_t* data, uint16_t length, const uint8_t* hmac);
    
    /**
     * @brief Check and update sequence number (replay prevention)
     */
    bool validateSequence(uint32_t sequence);
};

// Global security manager instance
extern SecurityManager securityManager;

#endif // SECURITY_H
