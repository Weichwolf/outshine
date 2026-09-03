#include "Log.h"
#include <array>
#include <cstdio>
#include <string>
#include <span>
#include <utility>

namespace outshine {

LogField::LogField(const char *key, double v) : Key(key) {
  std::array<char, 32> b{};
  snprintf(b.data(), b.size(), "%g", v);
  Value = b.data();
}

LogField::LogField(const char *key, int v) : Key(key), Value(std::to_string(v)) {}

LogField::LogField(const char *key, long long v) : Key(key), Value(std::to_string(v)) {}

LogField::LogField(const char *key, bool v) : Key(key), Value(v ? "1" : "0") {}

LogField::LogField(const char *key, const char *v) : Key(key), Value(v) {}

LogField::LogField(const char *key, std::string v) : Key(key), Value(std::move(v)) {}

LogSink *Log::Sink_ = nullptr;

LogLevel Log::Level_ = LogLevel::Debug;
thread_local LogSink *Log::ThreadSink_ = nullptr;
thread_local double Log::TimeS_ = 0.0;
thread_local std::array<char, 32> Log::Unit_ = {};

void Log::SetUnit(const char *label) {
  if (label == nullptr) {
    Unit_[0] = 0;
    return;
  }
  snprintf(Unit_.data(), Unit_.size(), "%s", label);
}

void Log::Emit(LogLevel level,
               const char *tag,
               const char *event,
               std::span<const LogField> fields) {
  if ((Sink_ == nullptr) || level < Level_) { return; }

  LogSink *out = (ThreadSink_ != nullptr) ? ThreadSink_ : Sink_;
  out->Write(TimeS_,
             level,
             {.Unit = (Unit_[0] != 0) ? Unit_.data() : nullptr, .Tag = tag, .Event = event},
             fields);
}

} // namespace outshine
