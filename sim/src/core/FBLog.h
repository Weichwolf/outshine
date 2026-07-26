/* FlightBox — FBLog: the ONE channel for discrete, greppable events (core/ architecture: Telemetrie =
 * periodic sampled state, Log = discrete events — see FBTelemetry.h for the former). A static facade,
 * not an owned object: logging is cross-cutting infrastructure every layer needs (systems/render/
 * world/fdm), and threading an FBLog& through every Run() signature would touch the whole call graph
 * for no behavioral gain — call sites stay a one-liner
 * (`FBLog::Warn("pilot", "sink_rate_high", {{"vs", -12.3}})`). All I/O lives in an INJECTED FBLogSink;
 * core/systems/render/world/fdm never touch a FILE pointer or fstream, only format field values (allowed —
 * "Core bleibt I/O-frei", CLAUDE.md). No sink set = free (one pointer check, no formatting work).
 * Concrete sinks (stdout/file/fan-out) live in app/ (FBLogSinks.h) — the one place raw stdio is
 * allowed, per CLAUDE.md's Engineering-Konventionen exception. */
#ifndef FBLOG_H
#define FBLOG_H
#include <initializer_list>
#include <string>
#include <vector>

namespace FlightBox {

enum class FBLogLevel { Debug, Info, Warn, Error };

/* One key=val field of a structured log line. Numeric overloads format compactly (%g); string/bool
 * forward as-is — the sink quotes a value containing whitespace (mirrors the legacy events.log
 * `reason="..."` convention). */
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
  /* The app updates this once per tick (sim-seconds, Prinzip 4) so every log line in that tick carries
   * a consistent, correlatable timestamp without threading `t` through every call site. */
  static void SetTime(double simTimeS) { TimeS_ = simTimeS; }

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

  static FBLogSink *Sink_;
  static FBLogLevel Level_;
  static double TimeS_;
};

} // namespace FlightBox
#endif /* FBLOG_H */
