#include "Telemetry.h"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace outshine {

void TelemetryRow::Push(double v) {
  std::array<char, 32> b{};
  snprintf(b.data(), b.size(), "%.6f", v);
  Fields_.emplace_back(b.data());
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
  for (const auto *src : Sources_) { src->DeclareTelemetry(Schema_); }
  if (Sink_ != nullptr) {
    std::vector<std::string> cols;
    cols.reserve(Schema_.Channels().size());
    for (const auto &c : Schema_.Channels()) { cols.push_back(c.Name); }
    Sink_->Header(cols);
  }
  Started_ = true;
}

void TelemetryBus::Tick(double simTimeS) {
  if (!Started_) { Start(); }
  if (Sink_ == nullptr) { return; }
  Row_.Clear();
  Row_.Push(simTimeS);
  for (const auto *src : Sources_) { src->SampleTelemetry(Row_); }
  Sink_->Row(Row_.Fields());
}

} // namespace outshine
