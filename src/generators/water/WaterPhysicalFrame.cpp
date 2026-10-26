#include "WaterPhysicalFrame.h"
#include "Geodesy.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <limits>

namespace outshine::Generators {
namespace {
constexpr double kMinimumNormalRadiusM = kWgs84A * (1 - kWgs84E2);
constexpr double kRequiredTargetUp = .9;
constexpr double kNormalMargin = 1e-12;
constexpr double kLargestNormalDelta = .01;
constexpr int kTargetPasses = 6;
constexpr int kRoundoffUlps = 64;

double EarthRoundoffM() {
  return kRoundoffUlps *
         (std::nextafter(kWgs84A, std::numeric_limits<double>::infinity()) - kWgs84A);
}

bool Finite(const Vec3 &point) {
  return std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(point[2]);
}

bool ValidFrame(const TangentFrame &frame) {
  const auto at = frame.Anchor();
  return std::isfinite(at.LongitudeDeg) && std::isfinite(at.LatitudeDeg) &&
         std::abs(at.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2;
}
}

std::expected<WaterGeodeticPoint, WaterFrameError> GeodeticWaterPoint(const TangentFrame &frame,
                                                                      const Vec3 &point) {
  if (!ValidFrame(frame) || !Finite(point)) {
    return std::unexpected(WaterFrameError::InvalidInput);
  }
  const auto ecef = frame.ToEcefPosition({.EastM = point[0], .NorthM = point[1], .UpM = point[2]});
  if (!Finite(ecef)) { return std::unexpected(WaterFrameError::InvalidInput); }
  const auto geographic = GeoFromEcef(ecef);
  if (!geographic || geographic->HeightM <= -kMinimumNormalRadiusM) {
    return std::unexpected(WaterFrameError::OutsideLocalDomain);
  }
  WaterGeodeticPoint result{.Geographic = *geographic, .Up = {}};
  const auto up = frame.ToLocalDirection(EnuAxesEcef(*geographic).Up);
  result.Up = {{up.EastM, up.NorthM, up.UpM}};
  if (!Finite(result.Up)) { return std::unexpected(WaterFrameError::OutsideLocalDomain); }
  return result;
}

std::expected<double, WaterFrameError>
WaterHeightTarget(const TangentFrame &frame, EastNorth point, double modelHeightM) {
  if (!std::isfinite(modelHeightM)) { return std::unexpected(WaterFrameError::InvalidInput); }
  Vec3 placed{{point.EastM, point.NorthM, modelHeightM}};
  for (int pass = 0; pass < kTargetPasses; ++pass) {
    const auto geographic = GeodeticWaterPoint(frame, placed);
    if (!geographic) { return std::unexpected(geographic.error()); }
    if (geographic->Up[2] < kRequiredTargetUp) {
      return std::unexpected(WaterFrameError::OutsideLocalDomain);
    }
    placed[2] += (modelHeightM - geographic->Geographic.HeightM) / geographic->Up[2];
  }
  const auto geographic = GeodeticWaterPoint(frame, placed);
  if (!geographic) { return std::unexpected(geographic.error()); }
  if (std::abs(geographic->Geographic.HeightM - modelHeightM) > EarthRoundoffM()) {
    return std::unexpected(WaterFrameError::NoConvergence);
  }
  return placed[2];
}

std::expected<WaterNormalCap, WaterFrameError>
BoundWaterFacetNormals(const TangentFrame &frame,
                       const std::array<std::array<double, 2>, 3> &triangle,
                       std::array<double, 2> upBoundsM) {
  const auto [low, high] = upBoundsM;
  if (!std::isfinite(low) || !std::isfinite(high) || low <= -kMinimumNormalRadiusM || low >= high) {
    return std::unexpected(WaterFrameError::InvalidInput);
  }
  WaterNormalCap result;
  for (const auto &point : triangle) {
    if (!std::isfinite(point[0]) || !std::isfinite(point[1])) {
      return std::unexpected(WaterFrameError::InvalidInput);
    }
    result.Center[0] += point[0];
    result.Center[1] += point[1];
  }
  result.Center[0] /= triangle.size();
  result.Center[1] /= triangle.size();
  result.Center[2] = low / 2 + high / 2;
  double radiusSquared = 0;
  for (const auto &point : triangle) {
    const double east = point[0] - result.Center[0];
    const double north = point[1] - result.Center[1];
    radiusSquared = std::max(radiusSquared, east * east + north * north);
  }
  const double halfHeight = high / 2 - low / 2;
  result.RadiusM = std::sqrt(radiusSquared + halfHeight * halfHeight);
  const auto geographic = GeodeticWaterPoint(frame, result.Center);
  if (!geographic) { return std::unexpected(geographic.error()); }
  result.Up = geographic->Up;
  double error = 0;
  if (low <= 0) {
    const auto reconstructed = frame.ToLocalPosition(geographic->Geographic);
    error = Length(Vec3{{reconstructed.EastM, reconstructed.NorthM, reconstructed.UpM}} -
                   result.Center) +
            EarthRoundoffM();
  }
  const double denominator = kMinimumNormalRadiusM + low - error;
  if (denominator <= 0) { return std::unexpected(WaterFrameError::OutsideLocalDomain); }
  result.Delta = (result.RadiusM + error) / denominator + kNormalMargin;
  if (!std::isfinite(result.Delta) || result.Delta >= kLargestNormalDelta) {
    return std::unexpected(WaterFrameError::OutsideLocalDomain);
  }
  return result;
}
}
