#include "LogSinks.h"
#include <print>
#include <span>
#include <cstdio>

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
                        Saying who,
                        std::span<const LogField> fields) {
  const char *const unit = who.Unit;
  const char *const tag = nameOf(who.Tag);
  const char *const event = who.Event;
  if (File_ == nullptr) { return; }
  std::print(File_, "t={:.1f} {} {} {}", simTimeS, LevelStr(level), tag, event);
  if (unit != nullptr) { std::print(File_, " unit={}", unit); }
  for (const auto &fld : fields) {
    if (fld.Value.contains(' ')) {
      std::print(File_, " {}=\"{}\"", fld.Key, fld.Value);
    } else {
      std::print(File_, " {}={}", fld.Key, fld.Value);
    }
  }
  std::println(File_, "");
  fflush(File_);
}

} // namespace outshine
