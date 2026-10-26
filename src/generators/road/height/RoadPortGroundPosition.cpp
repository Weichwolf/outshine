#include "RoadPortFrame.h"
#include "TangentFrame.h"
#include "Units.h"
#include "Vec3.h"
#include <cmath>
#include <expected>

namespace outshine::Generators {
namespace {
constexpr double kCoordinateMostM = 1e6;
constexpr double kLongitudeMostDeg = 180;
constexpr double kLatitudeMostDeg = 90;

Vec3 Scaled(const Vec3 &point) noexcept {
  return {{point[0] / kWgs84A, point[1] / kWgs84A, point[2] / (kWgs84A * (1 - kWgs84F))}};
}
}

std::expected<LongitudeLatitude, RoadPortQueryError>
RoadPortGroundPosition(const TangentFrame &source, EastNorth at) noexcept {
  const auto anchor = source.Anchor();
  if (!std::isfinite(anchor.LongitudeDeg) || !std::isfinite(anchor.LatitudeDeg) ||
      std::abs(anchor.LongitudeDeg) > kLongitudeMostDeg ||
      std::abs(anchor.LatitudeDeg) > kLatitudeMostDeg || !std::isfinite(at.EastM) ||
      !std::isfinite(at.NorthM) || std::abs(at.EastM) > kCoordinateMostM ||
      std::abs(at.NorthM) > kCoordinateMostM) {
    return std::unexpected(RoadPortQueryError::InvalidInput);
  }
  const Vec3 base = source.ToEcefPosition({.EastM = at.EastM, .NorthM = at.NorthM});
  const Vec3 scaledBase = Scaled(base);
  const Vec3 scaledUp = Scaled(source.UpEcef());
  const double quadratic = Dot(scaledUp, scaledUp);
  const double linear = 2 * Dot(scaledBase, scaledUp);
  const double constant = Dot(scaledBase, scaledBase) - 1;
  const double discriminant = linear * linear - 4 * quadratic * constant;
  if (!(discriminant > 0)) { return std::unexpected(RoadPortQueryError::UnqualifiedFrame); }
  const double height = -2 * constant / (linear + std::sqrt(discriminant));
  const Vec3 ground = base + source.UpEcef() * height;
  const LongitudeLatitude result{
      .LongitudeDeg = std::atan2(ground[1], ground[0]) / kDeg2Rad,
      .LatitudeDeg =
          std::atan2(ground[2], (1 - kWgs84E2) * std::hypot(ground[0], ground[1])) / kDeg2Rad};
  if (!std::isfinite(height) || !std::isfinite(result.LongitudeDeg) ||
      !std::isfinite(result.LatitudeDeg)) {
    return std::unexpected(RoadPortQueryError::UnqualifiedFrame);
  }
  return result;
}
}
