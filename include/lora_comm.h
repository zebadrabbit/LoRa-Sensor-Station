#ifndef LORA_COMM_H
#define LORA_COMM_H

#include "LoRaWan_APP.h"
#include "data_types.h"

// Initialize LoRa radio
void initLoRa();

// LoRa state management
bool isLoRaIdle();
void setLoRaIdle(bool idle);

// Radio event callbacks
void OnTxDone();
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnTxTimeout();
void OnRxTimeout();
void OnRxError();

// LoRa communication functions
void sendSensorData(const SensorData& data);
void enterRxMode();

#ifdef BASE_STATION
void handlePendingWebSocketBroadcast();  // Handle WebSocket broadcast from main loop
void checkCommandRetries();  // Check for commands that need retry
#endif

#ifdef SENSOR_NODE
bool shouldSendImmediateAck();  // Check if immediate ACK telemetry should be sent
uint32_t getEffectiveTransmitInterval(uint32_t configuredInterval);  // Get effective interval (may be forced after command)
#endif

#endif // LORA_COMM_H
