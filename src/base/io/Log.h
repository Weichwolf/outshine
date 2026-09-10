#ifndef OUTSHINE_BASE_IO_LOG_H
#define OUTSHINE_BASE_IO_LOG_H
#include <array>
#include <span>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "Logging.h"

namespace outshine {

class Log {
public:
  static void SetSink(LogSink *sink) { Sink_ = sink; }

  static void SetLevel(LogLevel level) { Level_ = level; }

  static void SetTime(double simTimeS) { TimeS_ = simTimeS; }

  static void SetUnit(std::string_view label) noexcept;

  static void SetThreadSink(LogSink *sink) { ThreadSink_ = sink; }

  static void Debug(LogTag tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Debug, tag, event, {fields.begin(), fields.size()});
  }

  static void Debug(LogTag tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Debug, tag, event, fields);
  }

  static void Info(LogTag tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Info, tag, event, {fields.begin(), fields.size()});
  }

  static void Info(LogTag tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Info, tag, event, fields);
  }

  static void Warn(LogTag tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Warn, tag, event, {fields.begin(), fields.size()});
  }

  static void Warn(LogTag tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Warn, tag, event, fields);
  }

  static void Error(LogTag tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Error, tag, event, {fields.begin(), fields.size()});
  }

  static void Error(LogTag tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Error, tag, event, fields);
  }

private:
  friend class LogUnitScope;
  friend class LogThreadSinkScope;

  static void Emit(LogLevel level, LogTag tag, const char *event, std::span<const LogField> fields);

  static LogSink *Sink_;
  static LogLevel Level_;
  static thread_local LogSink *ThreadSink_;
  static thread_local double TimeS_;
  static thread_local std::array<char, 32> Unit_;
};

class LogUnitScope {
public:
  explicit LogUnitScope(std::string_view label) noexcept : Previous_(Log::Unit_) {
    Log::SetUnit(label);
  }

  ~LogUnitScope() { Log::Unit_ = Previous_; }

  LogUnitScope(const LogUnitScope &) = delete;
  LogUnitScope &operator=(const LogUnitScope &) = delete;

private:
  decltype(Log::Unit_) Previous_;
};

class LogThreadSinkScope {
public:
  explicit LogThreadSinkScope(LogSink *sink) noexcept { Log::SetThreadSink(sink); }

  ~LogThreadSinkScope() { Log::SetThreadSink(Previous_); }

  LogThreadSinkScope(const LogThreadSinkScope &) = delete;
  LogThreadSinkScope &operator=(const LogThreadSinkScope &) = delete;

private:
  LogSink *Previous_ = Log::ThreadSink_;
};

}
#endif
