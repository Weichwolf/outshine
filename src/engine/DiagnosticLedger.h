#ifndef OUTSHINE_ENGINE_DIAGNOSTICLEDGER_H
#define OUTSHINE_ENGINE_DIAGNOSTICLEDGER_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <diagnostics/DiagnosticSample.h>

namespace outshine::Core {

class DiagnosticLedger {
public:
  void ConfigureMetrics(std::vector<DiagnosticSample> declared) {
    Samples_ = std::move(declared);
    ConfiguredSampleCount_ = Samples_.size();
    SampleFrameSerials_.assign(Samples_.size(), 0);
    FrameSerial_ = 0;
    ConflictingNames_.clear();
  }

  void BeginFrame() { ++FrameSerial_; }

  [[nodiscard]] const std::vector<std::string> &ConflictingMetricNames() const {
    return ConflictingNames_;
  }

  void RecordMetric(const std::string &what, double how, const char *unit) {
    RecordMetric(what.c_str(), how, unit);
  }

  void RecordMetric(const char *what, double how, const char *unit) {
    for (size_t at = ConfiguredSampleCount_; at < Samples_.size(); ++at) {
      if (Samples_[at].Name == what) {
        if (SampleFrameSerials_[at] == FrameSerial_) {
          if (Samples_[at].Value != how &&
              std::ranges::find(ConflictingNames_, what) == ConflictingNames_.end()) {
            ConflictingNames_.emplace_back(what);
          }
          return;
        }
        SampleFrameSerials_[at] = FrameSerial_;
        Samples_[at].Value = how;
        return;
      }
    }
    Samples_.push_back(DiagnosticSample{.Name = what, .Value = how, .Unit = unit});
    SampleFrameSerials_.push_back(FrameSerial_);
  }

  [[nodiscard]] const std::vector<DiagnosticSample> &Samples() const { return Samples_; }

private:
  std::vector<DiagnosticSample> Samples_;
  std::vector<uint64_t> SampleFrameSerials_;
  uint64_t FrameSerial_ = 0;
  std::vector<std::string> ConflictingNames_;
  size_t ConfiguredSampleCount_ = 0;
};

}
#endif
