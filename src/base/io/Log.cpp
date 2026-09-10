#include "Log.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
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

LogField::LogField(const char *key, std::string v) : Key(key), Value(std::move(v)) {}

LogSink *Log::Sink_ = nullptr;

LogLevel Log::Level_ = LogLevel::Debug;
thread_local LogSink *Log::ThreadSink_ = nullptr;
thread_local double Log::TimeS_ = 0.0;
thread_local std::array<char, 32> Log::Unit_ = {};

void Log::SetUnit(std::string_view label) noexcept {
  const size_t size = std::min(label.size(), Unit_.size() - 1);
  std::copy_n(label.begin(), size, Unit_.begin());
  Unit_[size] = 0;
}

void Log::Emit(LogLevel level, LogTag tag, const char *event, std::span<const LogField> fields) {
  if (level < Level_) { return; }
  LogSink *out = (ThreadSink_ != nullptr) ? ThreadSink_ : Sink_;
  if (out == nullptr) { return; }
  out->Write(TimeS_,
             level,
             {.Unit = (Unit_[0] != 0) ? Unit_.data() : nullptr, .Tag = tag, .Event = event},
             fields);
}

}
