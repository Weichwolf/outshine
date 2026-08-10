/* The ONE channel for discrete, greppable events (Telemetry.h is the other: periodic sampled state).
 * A static FACADE because logging is cross-cutting and an Log& through every Run() signature would
 * touch the whole call graph for no gain. I/O-free: nothing is emitted without an INJECTED LogSink,
 * and the concrete sinks live in app/.
 * THREADING: the CONFIGURATION (sink, level) is process-wide, the CONTEXT (time, unit, capture buffer)
 * is thread_local. */
#ifndef LOG_H
#define LOG_H
#include <initializer_list>
#include <string>
#include <vector>

namespace outshine {

enum class LogLevel { Debug, Info, Warn, Error };

/* One key=val field. Numeric overloads format compactly (%g); the SINK quotes a value with
 * whitespace (the events.log `reason="..."` convention). */
struct LogField {
  const char *Key;
  std::string Value;
  LogField(const char *key, double v);
  LogField(const char *key, int v);
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
  /* Updated once per tick, so every line in it carries one correlatable timestamp. */
  static void SetTime(double simTimeS) { TimeS_ = simTimeS; }

  /* WHICH actor a line is about. Empty adds NOTHING: a single-actor mission's lines are the mission's
   * and stay byte-identical to every pre-multi-unit baseline. */
  static void SetUnit(const char *label);

  /* Redirects THIS thread's output (null = back to the process sink). A worker that wrote straight
   * through to the shared log would make line order a function of the scheduler. */
  static void SetThreadSink(LogSink *sink) { ThreadSink_ = sink; }

  static void Debug(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
    Emit(LogLevel::Debug, tag, event, fields);
  }
  static void Info(const char *tag, const char *event, std::initializer_list<LogField> fields = {}) {
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

  static LogSink *Sink_;         /* boot configuration — process-wide on purpose */
  static LogLevel Level_;
  static thread_local LogSink *ThreadSink_;   /* emitting context */
  static thread_local double TimeS_;
  static thread_local char Unit_[32];   /* fixed buffer: changes per actor per tick, never allocates */
};

/* Scopes the unit attribution, so no actor's label leaks onto the next one's lines. */
class LogUnitScope {
public:
  explicit LogUnitScope(const std::string &label) { Log::SetUnit(label.c_str()); }
  ~LogUnitScope() { Log::SetUnit(nullptr); }
  LogUnitScope(const LogUnitScope &) = delete;
  LogUnitScope &operator=(const LogUnitScope &) = delete;
};

/* The same discipline for the capture buffer: a worker that returned without clearing it would keep
 * writing into it next tick — possibly into another unit's buffer. */
class LogThreadSinkScope {
public:
  explicit LogThreadSinkScope(LogSink *sink) { Log::SetThreadSink(sink); }
  ~LogThreadSinkScope() { Log::SetThreadSink(nullptr); }
  LogThreadSinkScope(const LogThreadSinkScope &) = delete;
  LogThreadSinkScope &operator=(const LogThreadSinkScope &) = delete;
};

} // namespace outshine
#endif /* LOG_H */
