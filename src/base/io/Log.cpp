#include "Log.h"
#include <cstdio>
#include <string>
#include <span>

namespace outshine {

LogField::LogField(const char *key, double v) : Key(key) {
  char b[32];
  snprintf(b, sizeof b, "%g", v);
  Value = b;
}

LogField::LogField(const char *key, int v) : Key(key), Value(std::to_string(v)) {}

LogField::LogField(const char *key, long long v) : Key(key), Value(std::to_string(v)) {}

LogField::LogField(const char *key, bool v) : Key(key), Value(v ? "1" : "0") {}

LogField::LogField(const char *key, const char *v) : Key(key), Value(v) {}

LogField::LogField(const char *key, const std::string &v) : Key(key), Value(v) {}

LogSink *Log::Sink_ = nullptr;

LogLevel Log::Level_ = LogLevel::Debug;
thread_local LogSink *Log::ThreadSink_ = nullptr;
thread_local double Log::TimeS_ = 0.0;
thread_local char Log::Unit_[32] = {0};

void Log::SetUnit(const char *label) {
  if (label == nullptr) {
    Unit_[0] = 0;
    return;
  }
  snprintf(Unit_, sizeof Unit_, "%s", label);
}

void Log::Emit(LogLevel level,
               const char *tag,
               const char *event,
               std::span<const LogField> fields) {
  if ((Sink_ == nullptr) || level < Level_) { return; }

  LogSink *out = (ThreadSink_ != nullptr) ? ThreadSink_ : Sink_;
  out->Write(TimeS_, level, (Unit_[0] != 0) ? Unit_ : nullptr, tag, event, fields);
}

} // namespace outshine
