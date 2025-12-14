/**
 * @file security.cpp
 * @brief Implementation of network security module
 */

#include "security.h"
#include "config_storage.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include <Preferences.h>
#include <esp_random.h>

// Global instance
SecurityManager securityManager;

SecurityManager::SecurityManager() {
    // Initialize with defaults
    config.encryptionEnabled = false;  // Disabled by default for backward compatibility
    config.whitelistEnabled = false;
    config.whitelistCount = 0;
    config.sequenceNumber = 0;
    
    // Generate random default key
    generateKey();
}

bool SecurityManager::begin() {
    Serial.println("🔒 Initializing security manager...");
    
    if (!loadConfig()) {
        Serial.println("⚠️ No security config found, using defaults");
        generateKey();
        saveConfig();
    }
    
    Serial.printf("🔒 Encryption: %s\n", config.encryptionEnabled ? "ENABLED" : "DISABLED");
    Serial.printf("🔒 Whitelist: %s (%d devices)\n", 
                  config.whitelistEnabled ? "ENABLED" : "DISABLED",
                  config.whitelistCount);
    
    return true;
}

void SecurityManager::generateKey() {
    Serial.println("🔑 Generating new encryption key...");
    
    // Use ESP32 hardware random number generator
    for (int i = 0; i < AES_KEY_SIZE; i++) {
        config.encryptionKey[i] = (uint8_t)esp_random();
    }
    
    Serial.print("🔑 Key: ");
    for (int i = 0; i < AES_KEY_SIZE; i++) {
        Serial.printf("%02X", config.encryptionKey[i]);
    }
    Serial.println();
}

void SecurityManager::setKey(const uint8_t* key) {
    memcpy(config.encryptionKey, key, AES_KEY_SIZE);
    Serial.println("🔑 Encryption key updated");
}

void SecurityManager::getKey(uint8_t* key) {
    memcpy(key, config.encryptionKey, AES_KEY_SIZE);
}

void SecurityManager::generateIV(uint8_t* iv) {
    // Generate random IV using hardware RNG
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        iv[i] = (uint8_t)esp_random();
    }
}

void SecurityManager::calculateHMAC(const uint8_t* data, uint16_t length, uint8_t* hmac) {
    // Simple HMAC using key XOR with data
    // For production, use proper HMAC-SHA256 (truncated)
    uint8_t hash[AES_BLOCK_SIZE] = {0};
    
    // XOR data with key repeatedly
    for (uint16_t i = 0; i < length; i++) {
        hash[i % AES_BLOCK_SIZE] ^= data[i];
    }
    
    // XOR with key again
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        hash[i] ^= config.encryptionKey[i % AES_KEY_SIZE];
    }
    
    // Copy first HMAC_SIZE bytes
    memcpy(hmac, hash, HMAC_SIZE);
}

bool SecurityManager::verifyHMAC(const uint8_t* data, uint16_t length, const uint8_t* hmac) {
    uint8_t calculated[HMAC_SIZE];
    calculateHMAC(data, length, calculated);
    
    // Constant-time comparison
    uint8_t diff = 0;
    for (int i = 0; i < HMAC_SIZE; i++) {
        diff |= (calculated[i] ^ hmac[i]);
    }
    
    return (diff == 0);
}

bool SecurityManager::validateSequence(uint32_t sequence) {
    // Accept sequence if it's greater than last seen
    // Allow some window for out-of-order packets
    const uint32_t SEQUENCE_WINDOW = 100;
    
    if (sequence > config.sequenceNumber) {
        config.sequenceNumber = sequence;
        return true;
    }
    
    // Allow recent packets within window
    if (config.sequenceNumber - sequence < SEQUENCE_WINDOW) {
        return true;
    }
    
    Serial.printf("⚠️ Replay detected: seq=%u, expected>%u\n", sequence, config.sequenceNumber);
    return false;
}

uint16_t SecurityManager::encryptPacket(const uint8_t* plaintext, uint16_t length,
                                        EncryptedPacket* encrypted, uint8_t sensorId, uint16_t networkId) {
    if (!config.encryptionEnabled || encrypted == nullptr) {
        return 0;
    }
    
    // Set header fields
    encrypted->packetType = 0xE0;  // Encrypted packet marker
    encrypted->sensorId = sensorId;
    encrypted->networkId = networkId;
    encrypted->sequenceNumber = ++config.sequenceNumber;
    
    // Generate random IV
    generateIV(encrypted->iv);
    
    // Pad plaintext to AES block size
    uint16_t paddedLength = ((length + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    uint8_t paddedData[256];
    memcpy(paddedData, plaintext, length);
    
    // PKCS7 padding
    uint8_t padValue = paddedLength - length;
    for (uint16_t i = length; i < paddedLength; i++) {
        paddedData[i] = padValue;
    }
    
    // Initialize mbedTLS AES context
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, config.encryptionKey, AES_KEY_SIZE * 8);
    
    // Encrypt data using AES-128-CBC
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, encrypted->iv, AES_BLOCK_SIZE);
    
    int ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, paddedLength,
                                     iv_copy, paddedData, encrypted->payload);
    
    mbedtls_aes_free(&aes_ctx);
    
    if (ret != 0) {
        Serial.printf("❌ AES encryption failed: %d\n", ret);
        return 0;
    }
    
    encrypted->payloadSize = paddedLength;
    
    // Calculate HMAC over header + encrypted payload
    uint8_t hmacData[256];
    uint16_t hmacLen = 0;
    hmacData[hmacLen++] = encrypted->packetType;
    hmacData[hmacLen++] = encrypted->sensorId;
    hmacData[hmacLen++] = (encrypted->networkId >> 8) & 0xFF;
    hmacData[hmacLen++] = encrypted->networkId & 0xFF;
    hmacData[hmacLen++] = (encrypted->sequenceNumber >> 24) & 0xFF;
    hmacData[hmacLen++] = (encrypted->sequenceNumber >> 16) & 0xFF;
    hmacData[hmacLen++] = (encrypted->sequenceNumber >> 8) & 0xFF;
    hmacData[hmacLen++] = encrypted->sequenceNumber & 0xFF;
    memcpy(&hmacData[hmacLen], encrypted->payload, paddedLength);
    hmacLen += paddedLength;
    
    calculateHMAC(hmacData, hmacLen, encrypted->hmac);
    
    Serial.printf("🔒 Encrypted packet: seq=%u, len=%d->%d bytes\n", 
                  encrypted->sequenceNumber, length, paddedLength);
    
    // Total size = header(16) + payload + IV(16)
    return 16 + paddedLength + AES_BLOCK_SIZE;
}

uint16_t SecurityManager::decryptPacket(const EncryptedPacket* encrypted, uint8_t* plaintext, uint16_t maxLength) {
    if (!config.encryptionEnabled || encrypted == nullptr) {
        return 0;
    }
    
    // Verify packet type
    if (encrypted->packetType != 0xE0) {
        Serial.println("⚠️ Invalid encrypted packet type");
        return 0;
    }
    
    // Check whitelist if enabled
    if (config.whitelistEnabled && !isWhitelisted(encrypted->sensorId)) {
        Serial.printf("⚠️ Sensor %d not in whitelist\n", encrypted->sensorId);
        return 0;
    }
    
    // Verify sequence number (replay prevention)
    if (!validateSequence(encrypted->sequenceNumber)) {
        return 0;
    }
    
    // Verify HMAC
    uint8_t hmacData[256];
    uint16_t hmacLen = 0;
    hmacData[hmacLen++] = encrypted->packetType;
    hmacData[hmacLen++] = encrypted->sensorId;
    hmacData[hmacLen++] = (encrypted->networkId >> 8) & 0xFF;
    hmacData[hmacLen++] = encrypted->networkId & 0xFF;
    hmacData[hmacLen++] = (encrypted->sequenceNumber >> 24) & 0xFF;
    hmacData[hmacLen++] = (encrypted->sequenceNumber >> 16) & 0xFF;
    hmacData[hmacLen++] = (encrypted->sequenceNumber >> 8) & 0xFF;
    hmacData[hmacLen++] = encrypted->sequenceNumber & 0xFF;
    memcpy(&hmacData[hmacLen], encrypted->payload, encrypted->payloadSize);
    hmacLen += encrypted->payloadSize;
    
    if (!verifyHMAC(hmacData, hmacLen, encrypted->hmac)) {
        Serial.println("⚠️ HMAC verification failed - packet tampered or wrong key");
        return 0;
    }
    
    // Initialize mbedTLS AES context
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, config.encryptionKey, AES_KEY_SIZE * 8);
    
    // Decrypt data using AES-128-CBC
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, encrypted->iv, AES_BLOCK_SIZE);
    
    uint8_t decrypted[256];
    int ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, encrypted->payloadSize,
                                     iv_copy, encrypted->payload, decrypted);
    
    mbedtls_aes_free(&aes_ctx);
    
    if (ret != 0) {
        Serial.printf("❌ AES decryption failed: %d\n", ret);
        return 0;
    }
    
    // Remove PKCS7 padding
    uint8_t padValue = decrypted[encrypted->payloadSize - 1];
    if (padValue > AES_BLOCK_SIZE || padValue == 0) {
        Serial.println("⚠️ Invalid padding");
        return 0;
    }
    
    uint16_t plaintextLen = encrypted->payloadSize - padValue;
    
    if (plaintextLen > maxLength) {
        Serial.println("⚠️ Decrypted data too large for buffer");
        return 0;
    }
    
    memcpy(plaintext, decrypted, plaintextLen);
    
    Serial.printf("🔓 Decrypted packet: seq=%u, len=%d bytes\n", 
                  encrypted->sequenceNumber, plaintextLen);
    
    return plaintextLen;
}

bool SecurityManager::addToWhitelist(uint8_t sensorId) {
    // Check if already in whitelist
    for (uint8_t i = 0; i < config.whitelistCount; i++) {
        if (config.whitelist[i] == sensorId) {
            Serial.printf("✓ Sensor %d already whitelisted\n", sensorId);
            return true;
        }
    }
    
    // Check capacity
    if (config.whitelistCount >= MAX_WHITELIST_SIZE) {
        Serial.println("⚠️ Whitelist full");
        return false;
    }
    
    // Add to list
    config.whitelist[config.whitelistCount++] = sensorId;
    Serial.printf("✓ Added sensor %d to whitelist (%d/%d)\n", 
                  sensorId, config.whitelistCount, MAX_WHITELIST_SIZE);
    
    return saveConfig();
}

bool SecurityManager::removeFromWhitelist(uint8_t sensorId) {
    // Find sensor in list
    for (uint8_t i = 0; i < config.whitelistCount; i++) {
        if (config.whitelist[i] == sensorId) {
            // Shift remaining entries
            for (uint8_t j = i; j < config.whitelistCount - 1; j++) {
                config.whitelist[j] = config.whitelist[j + 1];
            }
            config.whitelistCount--;
            
            Serial.printf("✓ Removed sensor %d from whitelist\n", sensorId);
            return saveConfig();
        }
    }
    
    Serial.printf("⚠️ Sensor %d not in whitelist\n", sensorId);
    return false;
}

bool SecurityManager::isWhitelisted(uint8_t sensorId) {
    if (!config.whitelistEnabled) {
        return true;  // Whitelist disabled, allow all
    }
    
    for (uint8_t i = 0; i < config.whitelistCount; i++) {
        if (config.whitelist[i] == sensorId) {
            return true;
        }
    }
    
    return false;
}

void SecurityManager::clearWhitelist() {
    config.whitelistCount = 0;
    Serial.println("✓ Whitelist cleared");
    saveConfig();
}

uint8_t SecurityManager::getWhitelist(uint8_t* list, uint8_t maxSize) {
    uint8_t count = (config.whitelistCount < maxSize) ? config.whitelistCount : maxSize;
    memcpy(list, config.whitelist, count);
    return count;
}

void SecurityManager::setEncryptionEnabled(bool enabled) {
    config.encryptionEnabled = enabled;
    Serial.printf("🔒 Encryption %s\n", enabled ? "ENABLED" : "DISABLED");
    saveConfig();
}

void SecurityManager::setWhitelistEnabled(bool enabled) {
    config.whitelistEnabled = enabled;
    Serial.printf("📋 Whitelist %s\n", enabled ? "ENABLED" : "DISABLED");
    saveConfig();
}

bool SecurityManager::saveConfig() {
    Preferences prefs;
    if (!prefs.begin("security", false)) {
        Serial.println("⚠️ Failed to open security storage");
        return false;
    }
    
    prefs.putBool("encrypt", config.encryptionEnabled);
    prefs.putBool("whitelist", config.whitelistEnabled);
    prefs.putBytes("key", config.encryptionKey, AES_KEY_SIZE);
    prefs.putUChar("wlCount", config.whitelistCount);
    prefs.putBytes("wlList", config.whitelist, config.whitelistCount);
    prefs.putUInt("seqNum", config.sequenceNumber);
    
    prefs.end();
    
    Serial.println("✓ Security config saved to NVS");
    return true;
}

bool SecurityManager::loadConfig() {
    Preferences prefs;
    if (!prefs.begin("security", true)) {
        return false;
    }
    
    if (!prefs.isKey("encrypt")) {
        prefs.end();
        return false;  // No config found
    }
    
    config.encryptionEnabled = prefs.getBool("encrypt", false);
    config.whitelistEnabled = prefs.getBool("whitelist", false);
    prefs.getBytes("key", config.encryptionKey, AES_KEY_SIZE);
    config.whitelistCount = prefs.getUChar("wlCount", 0);
    prefs.getBytes("wlList", config.whitelist, config.whitelistCount);
    config.sequenceNumber = prefs.getUInt("seqNum", 0);
    
    prefs.end();
    
    Serial.println("✓ Security config loaded from NVS");
    return true;
}
