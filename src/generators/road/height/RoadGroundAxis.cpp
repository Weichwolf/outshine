#include "RoadGroundStripInternal.h"
#include "GeosPolygon.h"
#include "PolygonTriangulation.h"
#include "TangentFrame.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>
#include <geos_c.h>

namespace outshine::Generators::RoadGround {
namespace {
constexpr double kAxisSegmentLeastM = 1e-6;
constexpr double kControlStepM = 20;
constexpr double kPlaneHalfBandM = 2;
constexpr double kPlaneHalfFraction = .2;
constexpr double kTurnMostRad = kPi / 6;

bool ValidOutline(const Axis &axis, double widthM) {
  PolygonRing ring;
  ring.reserve(2 * axis.Points.size());
  for (const double side : std::array{-1., 1.}) {
    for (size_t at = 0; at < axis.Points.size(); ++at) {
      const size_t index = side < 0 ? at : axis.Points.size() - 1 - at;
      ring.push_back({axis.Points[index].EastM + side * widthM / 2 * axis.Miters[index].EastM,
                      axis.Points[index].NorthM + side * widthM / 2 * axis.Miters[index].NorthM});
    }
  }
  auto *const context = GEOS_init_r();
  if (context == nullptr) { return false; }
  bool valid = false;
  {
    const auto polygon = Spatial::MakeGeosPolygon(context, std::span(&ring, 1));
    valid = polygon && GEOSisValid_r(context, polygon.get()) == 1 &&
            GEOSisEmpty_r(context, polygon.get()) == 0;
  }
  GEOS_finish_r(context);
  return valid;
}
}

bool Valid(LongitudeLatitude at) noexcept {
  return std::isfinite(at.LongitudeDeg) && std::isfinite(at.LatitudeDeg) &&
         std::abs(at.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2;
}

std::expected<Axis, RoadGroundError>
Frame(std::span<const LongitudeLatitude> geographic, double widthM, const std::stop_token &stop) {
  Axis result;
  result.Points.reserve(geographic.size());
  result.Stations.reserve(geographic.size());
  result.Tangents.reserve(geographic.size() - 1);
  result.Miters.reserve(geographic.size());
  const auto frame = TangentFrame::At(geographic.front());
  for (const auto at : geographic) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    if (!Valid(at)) { return std::unexpected(RoadGroundError::InvalidAxis); }
    result.Points.push_back(frame.ToLocalGroundPosition(at));
  }
  result.Stations.push_back(0);
  for (size_t at = 1; at < result.Points.size(); ++at) {
    const auto a = result.Points[at - 1];
    const auto b = result.Points[at];
    const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
    if (!std::isfinite(length) || length < kAxisSegmentLeastM) {
      return std::unexpected(RoadGroundError::InvalidAxis);
    }
    result.Tangents.push_back(
        {.EastM = (b.EastM - a.EastM) / length, .NorthM = (b.NorthM - a.NorthM) / length});
    result.Stations.push_back(result.Stations.back() + length);
  }
  const auto first = result.Tangents.front();
  result.Miters.push_back({.EastM = -first.NorthM, .NorthM = first.EastM});
  for (size_t at = 1; at < result.Tangents.size(); ++at) {
    const auto a = result.Tangents[at - 1];
    const auto b = result.Tangents[at];
    const double dot = a.EastM * b.EastM + a.NorthM * b.NorthM;
    if (dot < std::cos(kTurnMostRad)) {
      return std::unexpected(RoadGroundError::UnsupportedRecipe);
    }
    result.Miters.push_back(
        {.EastM = -(a.NorthM + b.NorthM) / (1 + dot), .NorthM = (a.EastM + b.EastM) / (1 + dot)});
  }
  const auto last = result.Tangents.back();
  result.Miters.push_back({.EastM = -last.NorthM, .NorthM = last.EastM});
  if (!ValidOutline(result, widthM)) { return std::unexpected(RoadGroundError::FoldedStrip); }
  return result;
}

std::expected<std::vector<double>, RoadGroundError> Bands(std::span<const double> stations,
                                                          size_t controlsMost) {
  const double end = stations.back();
  if (end < kControlStepM) { return std::unexpected(RoadGroundError::UnsupportedRecipe); }
  if (std::ceil(end / kControlStepM) - 1 > static_cast<double>(controlsMost)) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  std::vector<double> controls(stations.begin(), stations.end());
  const auto steps = static_cast<size_t>(std::ceil(end / kControlStepM));
  for (size_t at = 1; at < steps; ++at) {
    controls.push_back(static_cast<double>(at) * kControlStepM);
  }
  std::ranges::sort(controls);
  const auto repeated = std::ranges::unique(controls);
  controls.erase(repeated.begin(), controls.end());
  if (controls.size() > controlsMost) { return std::unexpected(RoadGroundError::CapacityExceeded); }
  std::vector<double> boundaries;
  boundaries.reserve(2 * controls.size());
  for (size_t at = 0; at < controls.size(); ++at) {
    const double before = at == 0 ? controls[1] - controls[0] : controls[at] - controls[at - 1];
    const double after = at + 1 == controls.size() ? before : controls[at + 1] - controls[at];
    const double half = std::min(kPlaneHalfBandM, kPlaneHalfFraction * std::min(before, after));
    boundaries.push_back(at == 0 ? 0 : controls[at] - half);
    boundaries.push_back(at + 1 == controls.size() ? end : controls[at] + half);
  }
  return boundaries;
}
}
