#include "logger.h"
#include <FS.h>
#include <LittleFS.h>
#include <stdarg.h>

static LoggerConfig g_cfg = {
  LOG_INFO,
  true,
  false,
  false,
  "/logs.txt",
  "/logs.txt"
};

static void formatTimestamp(char* buf, size_t len) {
  time_t now = time(nullptr);
  if (now > 1000) {
    struct tm tmnow;
    localtime_r(&now, &tmnow);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tmnow);
  } else {
    snprintf(buf, len, "1970-01-01 00:00:00");
  }
}

static const char* levelToStr(LogLevel level) {
  switch (level) {
    case LOG_ERROR: return "ERROR";
    case LOG_WARN:  return "WARN";
    case LOG_INFO:  return "INFO";
    case LOG_DEBUG: return "DEBUG";
  }
  return "INFO";
}

void loggerBegin(const LoggerConfig& cfg) {
  g_cfg = cfg;
  if (g_cfg.toLittleFS) {
    LittleFS.begin(true);
  }
}

void loggerSetLevel(LogLevel level) { g_cfg.level = level; }
LogLevel loggerGetLevel() { return g_cfg.level; }

static void writeToLittleFS(const String& line) {
  if (!g_cfg.toLittleFS) return;
  File f = LittleFS.open(g_cfg.littlefsPath ? g_cfg.littlefsPath : "/logs.txt", FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
}

static void writeToSD(const String& line) {
  // Stub: SD support to be implemented when SD is available
  // Leave function present so calls align across backends.
  (void)line;
}

void logMessage(LogLevel level, const char* tag, const char* msg) {
  if (level > g_cfg.level) return;
  char ts[24];
  formatTimestamp(ts, sizeof(ts));
  String line;
  line.reserve(128);
  line += ts;
  line += " ";
  line += levelToStr(level);
  line += " ";
  line += tag;
  line += ": ";
  line += msg;

  if (g_cfg.toSerial) Serial.println(line);
  writeToLittleFS(line);
  writeToSD(line);
}

void logf(LogLevel level, const char* tag, const char* fmt, ...) {
  if (level > g_cfg.level) return;
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  logMessage(level, tag, buf);
}
