#pragma once

namespace menu {

extern const char kLogTag[];

void LogInfo(const char *format, ...);
void LogWarn(const char *format, ...);
void LogError(const char *format, ...);

}  // namespace menu
