#include "LogSinks.h"

namespace outshine {

namespace {
const char *LevelStr(LogLevel l) {
  switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "?";
}
} // namespace

void TextLogSink::Write(double simTimeS,
                        LogLevel level,
                        const char *unit,
                        const char *tag,
                        const char *event,
                        std::span<const LogField> fields) {
  if (File_ == nullptr) { return; }
  fprintf(File_, "t=%.1f %s %s %s", simTimeS, LevelStr(level), tag, event);
  if (unit != nullptr) { fprintf(File_, " unit=%s", unit); }
  for (const auto &fld : fields) {
    if (fld.Value.find(' ') != std::string::npos) {
      fprintf(File_, " %s=\"%s\"", fld.Key, fld.Value.c_str());
    } else {
      fprintf(File_, " %s=%s", fld.Key, fld.Value.c_str());
    }
  }
  fprintf(File_, "\n");
  fflush(File_);
}

} // namespace outshine
