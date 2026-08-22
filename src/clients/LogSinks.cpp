#include "LogSinks.h"

namespace outshine::Clients {

namespace {
const char *LevelStr(LogLevel l) {
  switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "?";
}
}

void TextLogSink::Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                        std::span<const LogField> fields) {
  if (!File_) return;
  fprintf(File_, "t=%.1f %s %s %s", simTimeS, LevelStr(level), tag, event);
  for (const auto &fld : fields) {
    if (fld.Value.find(' ') != std::string::npos) fprintf(File_, " %s=\"%s\"", fld.Key, fld.Value.c_str());
    else fprintf(File_, " %s=%s", fld.Key, fld.Value.c_str());
  }
  fprintf(File_, "\n");
  fflush(File_);
}

}
