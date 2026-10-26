#include "UrbanArea.h"
#include "Units.h"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>

namespace outshine::Generators::Osm {
std::optional<UrbanLot> UrbanArea::LotAt(uint32_t column, uint32_t row) const noexcept {
  assert(column < Columns && row < Rows);
  assert(Tiers.size() == static_cast<size_t>(Columns) * Rows);
  const auto tier = Tiers[static_cast<size_t>(row) * Columns + column];
  assert(tier < HeightsM.size());
  if (tier == 0) { return std::nullopt; }
  const double pitch = Recipe.ProjectedPitchM;
  const double x = (static_cast<double>(FirstCell[0]) + column + .5) * pitch;
  const double y = (static_cast<double>(FirstCell[1]) - row + .5) * pitch;
  const double latitude = std::atan(std::sinh(y / kWgs84A));
  const double sine = std::sin(latitude);
  const double cosine = std::cos(latitude);
  constexpr double eccentricitySquared = kWgs84F * (2 - kWgs84F);
  const double denominator = 1 - eccentricitySquared * sine * sine;
  const double eastScale = cosine / std::sqrt(denominator);
  const double northScale = eastScale * (1 - eccentricitySquared) / denominator;
  constexpr double degrees = 180 / std::numbers::pi;
  const double footprint = pitch * Recipe.FillFraction;
  return UrbanLot{
      .Centre = {.LongitudeDeg = x / kWgs84A * degrees, .LatitudeDeg = latitude * degrees},
      .EastSpanM = footprint * eastScale,
      .NorthSpanM = footprint * northScale,
      .HeightM = HeightsM[tier]};
}
}
