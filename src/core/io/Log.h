#ifndef LOG_H
#define LOG_H
#include <initializer_list>
#include <string>
#include <vector>

namespace outshine {

enum class LogLevel { Debug, Info, Warn, Error };

struct LogField {
  const char *Key;
  std::string Value;
  LogField(const char *key, double v);
  LogField(const char *key, int v);

  LogField(const char *key, long long v);
  LogField(const char *key, bool v);
  LogField(const char *key, const char *v);
  LogField(const char *key, const std::string &v);
};

class LogSink {
public:
  virtual ~LogSink() = default;
  virtual void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                     const std::vector<LogField> &fields) = 0;
};

class Log {
public:
  static void SetSink(LogSink *sink) { Sink_ = sink; }
  static void SetLevel(LogLevel level) { Level_ = level; }

  static void SetTime(double simTimeS) { TimeS_ = simTimeS; }

  static void SetUnit(const char *label);

  static void SetThreadSink(LogSink *sink) { ThreadSink_ = sink; }

  static void Debug(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Debug, tag, event, fields);
  }
  static void Info(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Info, tag, event, fields);
  }

  static void Info(const char *tag, const char *event, const std::vector<LogField> &fields) {
    Emit(LogLevel::Info, tag, event, fields);
  }
  static void Warn(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Warn, tag, event, fields);
  }
  static void Error(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Error, tag, event, fields);
  }

private:
  static void Emit(LogLevel level, const char *tag, const char *event,
                   std::initializer_list<LogField> fields);
  static void Emit(LogLevel level, const char *tag, const char *event,
                   const std::vector<LogField> &fields);

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
