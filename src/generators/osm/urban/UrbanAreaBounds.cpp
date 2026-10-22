#include "UrbanArea.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <limits>
#include <numbers>
#include <numeric>
#include <string_view>

namespace outshine::Generators::Osm {
std::expected<UrbanPatchBounds, std::string_view>
UrbanArea::BoundPatch(UrbanPatchRange patch,
                      std::array<double, 2> groundAslMinMaxM) const noexcept {
  if (patch.Columns == 0 || patch.Rows == 0 || patch.Column >= Columns || patch.Row >= Rows ||
      patch.Columns > Columns - patch.Column || patch.Rows > Rows - patch.Row) {
    return std::unexpected("urban patch is outside its stored field");
  }
  if (!std::isfinite(groundAslMinMaxM[0]) || !std::isfinite(groundAslMinMaxM[1]) ||
      groundAslMinMaxM[0] > groundAslMinMaxM[1]) {
    return std::unexpected("urban patch requires a finite ground ASL interval");
  }
  const double pitch = Recipe.ProjectedPitchM;
  const double west = (static_cast<double>(FirstCell[0]) + patch.Column + .5) * pitch / kWgs84A;
  const double east = west + (patch.Columns - 1u) * pitch / kWgs84A;
  const double northY = (static_cast<double>(FirstCell[1]) - patch.Row + .5) * pitch / kWgs84A;
  const double southY = northY - (patch.Rows - 1u) * pitch / kWgs84A;
  const double north = std::atan(std::sinh(northY));
  const double south = std::atan(std::sinh(southY));
  const double bottom = groundAslMinMaxM[0];
  const double top = groundAslMinMaxM[1] + HeightsM.back();
  const double maxAbsHeight = std::max(std::abs(bottom), std::abs(top));
  const double derivative = kWgs84A / (1 - kWgs84F) + maxAbsHeight;
  const double footprintRadius =
      pitch * Recipe.FillFraction * std::numbers::sqrt2 / (2 * (1 - kWgs84F));
  double radius =
      derivative * ((east - west) + (north - south)) * .5 + (top - bottom) * .5 + footprintRadius;
  constexpr double degrees = 180 / std::numbers::pi;
  UrbanPatchBounds result;
  GeoToEcef({.LongitudeDeg = std::midpoint(west, east) * degrees,
             .LatitudeDeg = std::midpoint(south, north) * degrees,
             .HeightM = std::midpoint(bottom, top)},
            result.CentreEcef);
  constexpr double roundingAllowance = 64 * std::numeric_limits<double>::epsilon();
  radius += roundingAllowance * (Length(result.CentreEcef) + radius + kWgs84A + maxAbsHeight);
  result.RadiusM = std::nextafter(radius, std::numeric_limits<double>::infinity());
  if (!std::isfinite(result.RadiusM) ||
      !std::ranges::all_of(result.CentreEcef, [](double value) { return std::isfinite(value); })) {
    return std::unexpected("urban patch bounds exceed finite world coordinates");
  }
  return result;
}
}
