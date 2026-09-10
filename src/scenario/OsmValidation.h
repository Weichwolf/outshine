#ifndef OUTSHINE_SCENARIO_OSMVALIDATION_H
#define OUTSHINE_SCENARIO_OSMVALIDATION_H
#include <cmath>
#include <cstddef>
#include <expected>
#include <string_view>
#include <scenario/Scenario.h>
#include "math/Units.h"

namespace outshine {
namespace Says {
inline constexpr auto kInvalidOsmCoordinate =
    "OSM points require finite latitude,longitude pairs within WGS84 angular bounds";
inline constexpr auto kIncompleteOsmFeature =
    "OSM ways require two points and areas require three points";
inline constexpr auto kOsmPointBudget = "OSM feature exceeds the 65536 point preparation budget";
inline constexpr auto kInvalidOsmDimension =
    "OSM widthM and heightM require finite nonnegative metres";
inline constexpr auto kMissingOsmKind = "OSM feature requires a nonempty kind";
}

inline constexpr size_t kMaxOsmPoints = 65536;

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateOsmStructure(const Scenario::Structure &feature) noexcept {
  if (feature.Kind.empty()) { return std::unexpected(Says::kMissingOsmKind); }
  if (!std::isfinite(feature.WidthM) || feature.WidthM < 0 || !std::isfinite(feature.HeightM) ||
      feature.HeightM < 0) {
    return std::unexpected(Says::kInvalidOsmDimension);
  }
  const auto &points = feature.LatLon;
  if (points.size() > kMaxOsmPoints * 2) { return std::unexpected(Says::kOsmPointBudget); }
  if (points.size() % 2 != 0 || points.size() < (feature.Area ? 6u : 4u)) {
    return std::unexpected(Says::kIncompleteOsmFeature);
  }
  for (size_t at = 0; at < points.size(); at += 2) {
    if (!std::isfinite(points[at]) || std::abs(points[at]) > kDegPerHalfTurn / 2 ||
        !std::isfinite(points[at + 1]) || std::abs(points[at + 1]) > kDegPerHalfTurn) {
      return std::unexpected(Says::kInvalidOsmCoordinate);
    }
  }
  return {};
}
}
#endif
