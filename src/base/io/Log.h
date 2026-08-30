#ifndef OUTSHINE_BASE_IO_LOG_H
#define OUTSHINE_BASE_IO_LOG_H
#include <span>
#include <initializer_list>
#include <string>
#include <vector>

#include "Logging.h"

namespace outshine {

class Log {
public:
  static void SetSink(LogSink *sink) { Sink_ = sink; }
  static void SetLevel(LogLevel level) { Level_ = level; }

  static void SetTime(double simTimeS) { TimeS_ = simTimeS; }

  static void SetUnit(const char *label);

  static void SetThreadSink(LogSink *sink) { ThreadSink_ = sink; }

  static void Debug(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Debug, tag, event, {fields.begin(), fields.size()});
  }
  static void Debug(const char *tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Debug, tag, event, fields);
  }
  static void Info(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Info, tag, event, {fields.begin(), fields.size()});
  }
  static void Info(const char *tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Info, tag, event, fields);
  }

  static void Warn(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Warn, tag, event, {fields.begin(), fields.size()});
  }
  static void Warn(const char *tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Warn, tag, event, fields);
  }
  static void Error(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Error, tag, event, {fields.begin(), fields.size()});
  }
  static void Error(const char *tag, const char *event, std::span<const LogField> fields) {
    Emit(LogLevel::Error, tag, event, fields);
  }

private:
  static void Emit(LogLevel level, const char *tag, const char *event,
                   std::span<const LogField> fields);

  static LogSink *Sink_;
  static LogLevel Level_;
  static thread_local LogSink *ThreadSink_;
  static thread_local double TimeS_;
  static thread_local char Unit_[32];
};

class LogUnitScope {
public:
  explicit LogUnitScope(const std::string &label) { Log::SetUnit(label.c_str()); }
  ~LogUnitScope() { Log::SetUnit(nullptr); }
  LogUnitScope(const LogUnitScope &) = delete;
  LogUnitScope &operator=(const LogUnitScope &) = delete;
};

class LogThreadSinkScope {
public:
  explicit LogThreadSinkScope(LogSink *sink) { Log::SetThreadSink(sink); }
  ~LogThreadSinkScope() { Log::SetThreadSink(nullptr); }
  LogThreadSinkScope(const LogThreadSinkScope &) = delete;
  LogThreadSinkScope &operator=(const LogThreadSinkScope &) = delete;
};

}
#endif
