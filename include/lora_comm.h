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

#endif // LORA_COMM_H
