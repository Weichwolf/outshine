#include "FBLog.h"
#include <cstdio>

namespace FlightBox {

FBLogField::FBLogField(const char *key, double v) : Key(key) {
  char b[32];
  snprintf(b, sizeof b, "%g", v);
  Value = b;
}
FBLogField::FBLogField(const char *key, int v) : Key(key), Value(std::to_string(v)) {}
FBLogField::FBLogField(const char *key, bool v) : Key(key), Value(v ? "1" : "0") {}
FBLogField::FBLogField(const char *key, const char *v) : Key(key), Value(v) {}
FBLogField::FBLogField(const char *key, const std::string &v) : Key(key), Value(v) {}

FBLogSink *FBLog::Sink_ = nullptr;
/* Debug by default: every migrated call site used to print unconditionally, and CLAUDE.md's WASM
 * banner requires the browser console to look unchanged — the callers that want a quieter channel
 * (the mission runner's events.log) raise this explicitly. */
FBLogLevel FBLog::Level_ = FBLogLevel::Debug;
double FBLog::TimeS_ = 0.0;

void FBLog::Emit(FBLogLevel level, const char *tag, const char *event,
                 std::initializer_list<FBLogField> fields) {
  if (!Sink_ || level < Level_) return;
  Sink_->Write(TimeS_, level, tag, event, std::vector<FBLogField>(fields));
}

} // namespace FlightBox
