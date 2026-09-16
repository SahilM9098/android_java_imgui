#include "common/Log.h"

#include <android/log.h>
#include <cstdarg>

namespace menu {

const char kLogTag[] = "ImguiRenderer";

namespace {

void LogPrint(int priority, const char *format, va_list args) {
  __android_log_vprint(priority, kLogTag, format, args);
}

}  // namespace

void LogInfo(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogPrint(ANDROID_LOG_INFO, format, args);
  va_end(args);
}

void LogWarn(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogPrint(ANDROID_LOG_WARN, format, args);
  va_end(args);
}

void LogError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogPrint(ANDROID_LOG_ERROR, format, args);
  va_end(args);
}

}  // namespace menu
