#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <time.h>

enum LogLevel : uint8_t {
  LOG_ERROR = 0,
  LOG_WARN  = 1,
  LOG_INFO  = 2,
  LOG_DEBUG = 3
};

struct LoggerConfig {
  LogLevel level;
  bool toSerial;
  bool toLittleFS;
  bool toSD;
  const char* littlefsPath; // e.g., "/logs.txt"
  const char* sdPath;       // e.g., "/logs.txt"
};

void loggerBegin(const LoggerConfig& cfg);
void loggerSetLevel(LogLevel level);
LogLevel loggerGetLevel();

void logMessage(LogLevel level, const char* tag, const char* msg);
void logf(LogLevel level, const char* tag, const char* fmt, ...);

// Convenience macros
#define LOGE(tag, fmt, ...) logf(LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) logf(LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) logf(LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) logf(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
