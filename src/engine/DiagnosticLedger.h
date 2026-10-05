#ifndef OUTSHINE_ENGINE_DIAGNOSTICLEDGER_H
#define OUTSHINE_ENGINE_DIAGNOSTICLEDGER_H

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <diagnostics/DiagnosticSample.h>

namespace outshine::Core {

class DiagnosticLedger {
public:
  void ConfigureMetrics(std::vector<DiagnosticSample> declared) {
    Samples_ = std::move(declared);
    SampleFrameSerials_.assign(Samples_.size(), 0);
    FrameSerial_ = 0;
    ConflictingNames_.clear();
    Indices_.clear();
  }

  void BeginFrame() { ++FrameSerial_; }

  [[nodiscard]] const std::vector<std::string> &ConflictingMetricNames() const {
    return ConflictingNames_;
  }

  void RecordMetric(const std::string &what, double how, const char *unit) {
    RecordMetric(what.c_str(), how, unit);
  }

  void RecordMetric(const char *what, double how, const char *unit) {
    const auto found = Indices_.find(std::string_view(what));
    if (found != Indices_.end()) {
      const size_t at = found->second;
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
    Indices_.emplace(what, Samples_.size());
    Samples_.push_back(DiagnosticSample{.Name = what, .Value = how, .Unit = unit});
    SampleFrameSerials_.push_back(FrameSerial_);
  }

  [[nodiscard]] const std::vector<DiagnosticSample> &Samples() const { return Samples_; }

private:
  std::vector<DiagnosticSample> Samples_;
  std::vector<uint64_t> SampleFrameSerials_;
  uint64_t FrameSerial_ = 0;
  std::vector<std::string> ConflictingNames_;

  struct NameHash {
    using is_transparent = void;

    size_t operator()(std::string_view name) const noexcept {
      return std::hash<std::string_view>{}(name);
    }
  };

  std::unordered_map<std::string, size_t, NameHash, std::equal_to<>> Indices_;
};

}
#endif
