#include "CaseInput.h"
#include "FixedPatch.h"
#include "UncertainPatch.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CoverageCases {
namespace F = FixedCoverage;

namespace {

std::expected<std::array<float, 4>, std::string> ReadPoint(const nlohmann::json &point) {
  if (!point.is_array() || point.size() != 4) {
    return std::unexpected("invalid clip-space point");
  }
  std::array<float, 4> result{};
  for (size_t i = 0; i < result.size(); ++i) {
    if (!point[i].is_number()) { return std::unexpected("non-numeric clip coordinate"); }
    const double value = point[i].get<double>();
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
      return std::unexpected("coordinate outside finite float32");
    }
    result[i] = static_cast<float>(value);
  }
  return result;
}

std::expected<F::SourceQuad, std::string> ReadQuad(const nlohmann::json &source) {
  if (!source.is_object() || !source.contains("points") || !source["points"].is_array() ||
      source["points"].size() != 4) {
    return std::unexpected("source requires four corners");
  }
  F::SourceQuad quad;
  quad.Opaque = true;
  quad.Complete = true;
  for (size_t i = 0; i < quad.Points.size(); ++i) {
    auto point = ReadPoint(source["points"][i]);
    if (!point) { return std::unexpected(point.error()); }
    quad.Points[i] = *point;
  }
  return quad;
}

std::expected<F::UncertainQuad, std::string> ReadUncertain(const nlohmann::json &source) {
  const auto &corners = source["clip_bounds"];
  if (!corners.is_array() || corners.size() != 4 || !source.contains("front_sign") ||
      !source["front_sign"].is_number_integer() ||
      (source["front_sign"] != -1 && source["front_sign"] != 1)) {
    return std::unexpected("invalid uncertainty/facing fields");
  }
  F::UncertainQuad result{
      .FrontWindowOrientation = source["front_sign"].get<int>(), .Opaque = true, .Complete = true};
  for (size_t i = 0; i < 4; ++i) {
    if (!corners[i].is_array() || corners[i].size() != 4) {
      return std::unexpected("invalid clip bounds");
    }
    for (size_t axis = 0; axis < 4; ++axis) {
      const auto &range = corners[i][axis];
      if (!range.is_array() || range.size() != 2 || !range[0].is_number() ||
          !range[1].is_number()) {
        return std::unexpected("invalid interval");
      }
      const double low = range[0].get<double>();
      const double high = range[1].get<double>();
      if (!std::isfinite(low) || !std::isfinite(high) || low > high) {
        return std::unexpected("invalid interval bounds");
      }
      result.Clip[i][axis] = {.Lower = low, .Upper = high};
    }
  }
  return result;
}

std::expected<void, std::string> ReadRequirements(const nlohmann::json &entry, Case &fixture) {
  constexpr uint32_t MaximumPixels = 64 * 64;
  for (const auto *const name : {"required_pixels", "required_minimum"}) {
    if (!entry.contains(name)) { continue; }
    const auto &value = entry[name];
    if (!value.is_number_integer() || value < 0 || value > MaximumPixels) {
      return std::unexpected("invalid expected pixel count");
    }
    if (std::string_view(name) == "required_pixels") {
      fixture.RequiredPixels = value.get<uint32_t>();
    } else {
      fixture.RequiredMinimum = value.get<uint32_t>();
    }
  }
  if (entry.contains("required_unknown")) {
    const auto &value = entry["required_unknown"];
    if (!value.is_number_integer() || value < 0 ||
        value > static_cast<int>(F::Unknown::EmptyInterior)) {
      return std::unexpected("invalid expected rejection");
    }
    fixture.RequiredUnknown = static_cast<F::Unknown>(value.get<int>());
  }
  if (entry.contains("required_depth")) {
    const auto &value = entry["required_depth"];
    if (!value.is_number() || value < 0 || value > 1) {
      return std::unexpected("invalid expected depth");
    }
    fixture.RequiredDepth = value.get<float>();
  }
  return {};
}

std::expected<Case, std::string> ReadCase(const nlohmann::json &entry) {
  constexpr size_t SourceLimit = 4096;
  constexpr size_t NameLimit = 128;
  if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string() ||
      !entry.contains("sources") || !entry["sources"].is_array() ||
      entry["sources"].size() > SourceLimit) {
    return std::unexpected("invalid case fields");
  }
  Case fixture{.Name = entry["name"].get<std::string>(),
               .Sources = {},
               .UncertainSources = {},
               .ExternalReference = true};
  if (fixture.Name.empty() || fixture.Name.size() > NameLimit ||
      !std::ranges::all_of(
          fixture.Name, [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; })) {
    return std::unexpected("case name must be a filename-safe identifier");
  }
  if (entry.contains("integer_certificate")) {
    if (!entry["integer_certificate"].is_boolean()) {
      return std::unexpected("invalid integer-certificate policy");
    }
    fixture.IntegerCertificate = entry["integer_certificate"].get<bool>();
  }
  const auto requirements = ReadRequirements(entry, fixture);
  if (!requirements) { return std::unexpected(requirements.error()); }
  for (const auto &source : entry["sources"]) {
    if (source.is_object() && source.contains("clip_bounds")) {
      auto uncertain = ReadUncertain(source);
      if (!uncertain) { return std::unexpected(uncertain.error()); }
      fixture.UncertainSources.push_back(*uncertain);
      continue;
    }
    auto quad = ReadQuad(source);
    if (!quad) { return std::unexpected(quad.error()); }
    fixture.Sources.push_back(*quad);
  }
  return fixture;
}

}

std::expected<std::vector<Case>, std::string> ReadCases(const std::filesystem::path &path) {
  constexpr std::streamoff InputLimit = std::streamoff{8} * 1024 * 1024;
  constexpr size_t CaseLimit = 64;
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() < 0 || input.tellg() > InputLimit) {
    return std::unexpected("case input missing or too large");
  }
  input.seekg(0);
  const auto data = nlohmann::json::parse(input, nullptr, false);
  if (data.is_discarded() || !data.is_object() || !data.contains("cases") ||
      !data["cases"].is_array() || data["cases"].size() > CaseLimit ||
      !data.contains("qualified_for_page_suppression") ||
      data["qualified_for_page_suppression"] != false) {
    return std::unexpected("input must declare calibration-only cases");
  }
  std::vector<Case> cases;
  for (const auto &entry : data["cases"]) {
    auto fixture = ReadCase(entry);
    if (!fixture) { return std::unexpected(fixture.error()); }
    cases.push_back(std::move(*fixture));
  }
  return cases;
}

}
