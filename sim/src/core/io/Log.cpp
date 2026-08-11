#include "Log.h"
#include <cstdio>

namespace outshine {

LogField::LogField(const char *key, double v) : Key(key) {
  char b[32];
  snprintf(b, sizeof b, "%g", v);
  Value = b;
}
LogField::LogField(const char *key, int v) : Key(key), Value(std::to_string(v)) {}
LogField::LogField(const char *key, bool v) : Key(key), Value(v ? "1" : "0") {}
LogField::LogField(const char *key, const char *v) : Key(key), Value(v) {}
LogField::LogField(const char *key, const std::string &v) : Key(key), Value(v) {}

LogSink *Log::Sink_ = nullptr;
/* Debug by default so the browser console looks unchanged; a caller wanting a quieter channel raises
 * the level explicitly. */
LogLevel Log::Level_ = LogLevel::Debug;
thread_local LogSink *Log::ThreadSink_ = nullptr;
thread_local double Log::TimeS_ = 0.0;
thread_local char Log::Unit_[32] = {0};

void Log::SetUnit(const char *label) {
  if (!label) { Unit_[0] = 0; return; }
  snprintf(Unit_, sizeof Unit_, "%s", label);
}

void Log::Emit(LogLevel level, const char *tag, const char *event,
                 std::initializer_list<LogField> fields) {
  Emit(level, tag, event, std::vector<LogField>(fields));
}

void Log::Emit(LogLevel level, const char *tag, const char *event,
                 const std::vector<LogField> &fields) {
  if (!Sink_ || level < Level_) return;
  /* A capture buffer is a redirect of an already-accepted line, not a second switch. */
  LogSink *out = ThreadSink_ ? ThreadSink_ : Sink_;
  if (!Unit_[0]) {
    out->Write(TimeS_, level, tag, event, fields);
    return;
  }
  /* Attribution FIRST: a script splits on the first field, a human sees whose line it is at once. */
  std::vector<LogField> withUnit;
  withUnit.reserve(fields.size() + 1);
  withUnit.emplace_back("unit", static_cast<const char *>(Unit_));
  withUnit.insert(withUnit.end(), fields.begin(), fields.end());
  out->Write(TimeS_, level, tag, event, withUnit);
}

} // namespace outshine
