#include "Telemetry.h"
#include <cstdio>

namespace outshine {

void TelemetryRow::Push(double v) {
  char b[32];
  snprintf(b, sizeof b, "%.6f", v);
  Fields_.emplace_back(b);
}

void TelemetryRow::Push(int v) {
  Fields_.push_back(std::to_string(v));
}

void TelemetryRow::Push(long long v) {
  Fields_.push_back(std::to_string(v));
}

void TelemetryRow::Push(bool v) {
  Fields_.emplace_back(v ? "1" : "0");
}

void TelemetryRow::Push(const std::string &v) {
  Fields_.push_back(v);
}

void TelemetryBus::Start() {
  Schema_.Add("t", "s");
  for (auto *src : Sources_) { src->DeclareTelemetry(Schema_); }
  if (Sink_) {
    std::vector<std::string> cols;
    cols.reserve(Schema_.Channels().size());
    for (auto &c : Schema_.Channels()) { cols.push_back(c.Name); }
    Sink_->Header(cols);
  }
  Started_ = true;
}

void TelemetryBus::Tick(double simTimeS) {
  if (!Started_) { Start(); }
  if (!Sink_) { return; }
  Row_.Clear();
  Row_.Push(simTimeS);
  for (auto *src : Sources_) { src->SampleTelemetry(Row_); }
  Sink_->Row(Row_.Fields());
}

}
