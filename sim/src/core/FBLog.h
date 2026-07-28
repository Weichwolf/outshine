/* The ONE channel for discrete, greppable events (FBTelemetry.h is the other: periodic sampled state).
 * A static FACADE because logging is cross-cutting and an FBLog& through every Run() signature would
 * touch the whole call graph for no gain. I/O-free: nothing is emitted without an INJECTED FBLogSink,
 * and the concrete sinks live in app/.
 * THREADING: the CONFIGURATION (sink, level) is process-wide, the CONTEXT (time, unit, capture buffer)
 * is thread_local. doc/core.md, Abschnitt 3.1. */
#ifndef FBLOG_H
#define FBLOG_H
#include <initializer_list>
#include <string>
#include <vector>

namespace FlightBox {

enum class FBLogLevel { Debug, Info, Warn, Error };

/* One key=val field. Numeric overloads format compactly (%g); the SINK quotes a value with
 * whitespace (the events.log `reason="..."` convention). */
struct FBLogField {
  const char *Key;
  std::string Value;
  FBLogField(const char *key, double v);
  FBLogField(const char *key, int v);
  FBLogField(const char *key, bool v);
  FBLogField(const char *key, const char *v);
  FBLogField(const char *key, const std::string &v);
};

class FBLogSink {
public:
  virtual ~FBLogSink() = default;
  virtual void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
                     const std::vector<FBLogField> &fields) = 0;
};

class FBLog {
public:
  static void SetSink(FBLogSink *sink) { Sink_ = sink; }
  static void SetLevel(FBLogLevel level) { Level_ = level; }
  /* Updated once per tick, so every line in it carries one correlatable timestamp. */
  static void SetTime(double simTimeS) { TimeS_ = simTimeS; }

  /* WHICH actor a line is about. Empty adds NOTHING: a single-actor mission's lines are the mission's
   * and stay byte-identical to every pre-multi-unit baseline. */
  static void SetUnit(const char *label);

  /* Redirects THIS thread's output (null = back to the process sink). A worker that wrote straight
   * through to the shared log would make line order a function of the scheduler. */
  static void SetThreadSink(FBLogSink *sink) { ThreadSink_ = sink; }

  static void Debug(const char *tag, const char *event, std::initializer_list<FBLogField> fields = {}) {
    Emit(FBLogLevel::Debug, tag, event, fields);
  }
  static void Info(const char *tag, const char *event, std::initializer_list<FBLogField> fields = {}) {
    Emit(FBLogLevel::Info, tag, event, fields);
  }
  static void Warn(const char *tag, const char *event, std::initializer_list<FBLogField> fields = {}) {
    Emit(FBLogLevel::Warn, tag, event, fields);
  }
  static void Error(const char *tag, const char *event, std::initializer_list<FBLogField> fields = {}) {
    Emit(FBLogLevel::Error, tag, event, fields);
  }

private:
  static void Emit(FBLogLevel level, const char *tag, const char *event,
                   std::initializer_list<FBLogField> fields);

  static FBLogSink *Sink_;         /* boot configuration — process-wide on purpose */
  static FBLogLevel Level_;
  static thread_local FBLogSink *ThreadSink_;   /* emitting context */
  static thread_local double TimeS_;
  static thread_local char Unit_[32];   /* fixed buffer: changes per actor per tick, never allocates */
};

/* Scopes the unit attribution, so no actor's label leaks onto the next one's lines. */
class FBLogUnitScope {
public:
  explicit FBLogUnitScope(const std::string &label) { FBLog::SetUnit(label.c_str()); }
  ~FBLogUnitScope() { FBLog::SetUnit(nullptr); }
  FBLogUnitScope(const FBLogUnitScope &) = delete;
  FBLogUnitScope &operator=(const FBLogUnitScope &) = delete;
};

/* The same discipline for the capture buffer: a worker that returned without clearing it would keep
 * writing into it next tick — possibly into another unit's buffer. */
class FBLogThreadSinkScope {
public:
  explicit FBLogThreadSinkScope(FBLogSink *sink) { FBLog::SetThreadSink(sink); }
  ~FBLogThreadSinkScope() { FBLog::SetThreadSink(nullptr); }
  FBLogThreadSinkScope(const FBLogThreadSinkScope &) = delete;
  FBLogThreadSinkScope &operator=(const FBLogThreadSinkScope &) = delete;
};

} // namespace FlightBox
#endif /* FBLOG_H */
