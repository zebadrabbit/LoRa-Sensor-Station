#include "time_status.h"

#ifdef BASE_STATION
static volatile time_t g_lastNtpSyncEpoch = 0;
static volatile uint32_t g_lastTimeBroadcastMs = 0;

void timeSyncNotificationCb(struct timeval *tv) {
  if (tv) {
    g_lastNtpSyncEpoch = tv->tv_sec;
  }
}

void registerNtpTimeSyncCallback() { /* no-op fallback if SNTP callback not available */ }

void setLastTimeBroadcastMs(uint32_t ms) {
  g_lastTimeBroadcastMs = ms;
}

uint32_t getLastTimeBroadcastMs() {
  return g_lastTimeBroadcastMs;
}

time_t getLastNtpSyncEpoch() {
  return g_lastNtpSyncEpoch;
}

void setLastNtpSyncEpoch(time_t epoch) {
  g_lastNtpSyncEpoch = epoch;
}
#endif

#ifdef SENSOR_NODE
static volatile uint32_t g_sensorLastTimeSyncEpoch = 0;

void setSensorLastTimeSyncEpoch(uint32_t epoch) {
  g_sensorLastTimeSyncEpoch = epoch;
}

uint32_t getSensorLastTimeSyncEpoch() {
  return g_sensorLastTimeSyncEpoch;
}
#endif
