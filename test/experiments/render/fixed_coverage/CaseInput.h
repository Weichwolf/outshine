#ifndef OUTSHINE_EXPERIMENT_COVERAGE_CASE_INPUT_H
#define OUTSHINE_EXPERIMENT_COVERAGE_CASE_INPUT_H
#include "FixedPatch.h"
#include "UncertainPatch.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace CoverageCases {
struct Case {
  std::string Name;
  std::vector<FixedCoverage::SourceQuad> Sources;
  std::vector<FixedCoverage::UncertainQuad> UncertainSources;
  std::optional<uint32_t> RequiredPixels = std::nullopt;
  std::optional<uint32_t> RequiredMinimum = std::nullopt;
  std::optional<float> RequiredDepth = std::nullopt;
  std::optional<FixedCoverage::Unknown> RequiredUnknown = std::nullopt;
  bool ExternalReference = false;
  bool IntegerCertificate = false;
};

[[nodiscard]] std::expected<std::vector<Case>, std::string>
ReadCases(const std::filesystem::path &path);
}
#endif
