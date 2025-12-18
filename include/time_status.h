#ifndef TIME_STATUS_H
#define TIME_STATUS_H

#include <Arduino.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Base-station side: last NTP sync (epoch) and last broadcast (millis)
#ifdef BASE_STATION
void registerNtpTimeSyncCallback();
void timeSyncNotificationCb(struct timeval *tv);
void setLastTimeBroadcastMs(uint32_t ms);
uint32_t getLastTimeBroadcastMs();
time_t getLastNtpSyncEpoch();
void setLastNtpSyncEpoch(time_t epoch);
#endif

// Sensor side: last applied time sync epoch
#ifdef SENSOR_NODE
void setSensorLastTimeSyncEpoch(uint32_t epoch);
uint32_t getSensorLastTimeSyncEpoch();
#endif

#ifdef __cplusplus
}
#endif

#endif // TIME_STATUS_H
