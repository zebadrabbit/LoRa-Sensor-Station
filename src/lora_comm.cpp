#include "lora_comm.h"
#include "config.h"
#include "led_control.h"
#include "display_control.h"
#include "statistics.h"
#include <Arduino.h>

static RadioEvents_t RadioEvents;
static bool lora_idle = true;

void initLoRa() {
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  
  Radio.SetTxConfig(
    MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
    true, 0, 0, LORA_IQ_INVERSION_ON, 3000
  );
  
  Radio.SetRxConfig(
    MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
    sizeof(SensorData), true, 0, 0, LORA_IQ_INVERSION_ON, true
  );
  
  #ifdef BASE_STATION
    Serial.println("Base station ready - listening for sensors...");
    Serial.printf("Frequency: %d Hz\n", RF_FREQUENCY);
    Serial.printf("Bandwidth: %d, SF: %d, CR: %d\n", LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODINGRATE);
    Serial.printf("Expected packet size: %d bytes\n", sizeof(SensorData));
  #elif defined(SENSOR_NODE)
    Serial.println("Sensor node ready - preparing to send data...");
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
  lora_idle = true;
  #ifdef SENSOR_NODE
    blinkLED(getColorBlue(), 2, 100);
    Radio.Sleep();
  #endif
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  #ifdef BASE_STATION
    Serial.printf("RX: Received %d bytes, RSSI: %d, SNR: %d\n", size, rssi, snr);
    Radio.Sleep();
    
    recordRxPacket(rssi);
    
    if (size == sizeof(SensorData)) {
      SensorData received;
      memcpy(&received, payload, sizeof(SensorData));
      
      // Validate packet
      if (received.syncWord == SYNC_WORD && validateChecksum(&received)) {
        updateSensorInfo(received, rssi, snr);
        
        Serial.println("\n=== RECEIVED DATA ===");
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
        Serial.println("Invalid packet received");
        blinkLED(getColorRed(), 1, 100);
      }
    } else {
      recordRxInvalid();
      Serial.printf("Received packet with wrong size: %d bytes\n", size);
    }
    
    lora_idle = true;
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
}

void OnRxError() {
  Serial.println("RX Error");
  #ifdef BASE_STATION
    blinkLED(getColorRed(), 1, 50);
    lora_idle = true;
  #endif
}
