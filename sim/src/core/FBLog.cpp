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
char FBLog::Unit_[32] = {0};

void FBLog::SetUnit(const char *label) {
  if (!label) { Unit_[0] = 0; return; }
  snprintf(Unit_, sizeof Unit_, "%s", label);
}

void FBLog::Emit(FBLogLevel level, const char *tag, const char *event,
                 std::initializer_list<FBLogField> fields) {
  if (!Sink_ || level < Level_) return;
  if (!Unit_[0]) {
    Sink_->Write(TimeS_, level, tag, event, std::vector<FBLogField>(fields));
    return;
  }
  /* Attribution FIRST, so `unit=` reads before the payload on every attributed line — a script splits
   * on the first field, a human sees whose line it is without scanning to the end. */
  std::vector<FBLogField> withUnit;
  withUnit.reserve(fields.size() + 1);
  withUnit.emplace_back("unit", static_cast<const char *>(Unit_));
  withUnit.insert(withUnit.end(), fields.begin(), fields.end());
  Sink_->Write(TimeS_, level, tag, event, withUnit);
}

} // namespace FlightBox
