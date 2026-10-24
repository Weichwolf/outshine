#include "UrbanArea.h"
#include "Check.h"
#include "Geodesy.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = outshine::Generators::Osm;
constexpr double kPitchM = 512;
constexpr uint32_t kSide = 8;
constexpr double kDegrees = 180 / std::numbers::pi;
constexpr std::array<double, 2> kGroundRangeM{-400, 5000};

Osm::UrbanArea AreaAt(LongitudeLatitude focus) {
  Osm::UrbanArea area;
  area.Recipe.ProjectedPitchM = kPitchM;
  area.FirstCell = {
      static_cast<int64_t>(std::floor(kWgs84A * focus.LongitudeDeg / kDegrees / kPitchM)),
      static_cast<int64_t>(
          std::floor(kWgs84A * std::asinh(std::tan(focus.LatitudeDeg / kDegrees)) / kPitchM))};
  area.Columns = area.Rows = kSide;
  area.Tiers.assign(static_cast<size_t>(area.Columns) * area.Rows, 4);
  return area;
}

void EnclosedAt(LongitudeLatitude focus) {
  const auto area = AreaAt(focus);
  const Osm::UrbanPatchRange range{
      .Column = 1, .Row = 1, .Columns = area.Columns - 2, .Rows = area.Rows - 2};
  const auto bound = area.BoundPatch(range, kGroundRangeM);
  CHECK(bound, "selected patch obtains bounds from an explicit ground-height interval");
  if (!bound) { return; }
  for (uint32_t row = range.Row; row < range.Row + range.Rows; ++row) {
    for (uint32_t column = range.Column; column < range.Column + range.Columns; ++column) {
      const auto lot = area.LotAt(column, row);
      CHECK(lot, "the containment control contains generated lots");
      if (!lot) { continue; }
      const auto axes = EnuAxesEcef(
          {.LongitudeDeg = lot->Centre.LongitudeDeg, .LatitudeDeg = lot->Centre.LatitudeDeg});
      for (const auto height : kGroundRangeM) {
        Vec3 base;
        GeoToEcef({.LongitudeDeg = lot->Centre.LongitudeDeg,
                   .LatitudeDeg = lot->Centre.LatitudeDeg,
                   .HeightM = height},
                  base);
        for (const double east : {-lot->EastSpanM / 2, lot->EastSpanM / 2}) {
          for (const double north : {-lot->NorthSpanM / 2, lot->NorthSpanM / 2}) {
            for (const double up : {0.0, static_cast<double>(lot->HeightM)}) {
              const Vec3 point = base + axes.East * east + axes.North * north + axes.Up * up;
              CHECK(Length(point - bound->CentreEcef) <= bound->RadiusM,
                    "ECEF sphere encloses every plumb box corner at both ground extrema");
            }
          }
        }
      }
    }
  }
}

void InvalidInputs() {
  const auto area = AreaAt({.LongitudeDeg = 0, .LatitudeDeg = 0});
  const Osm::UrbanPatchRange range{.Column = 0, .Row = 0, .Columns = kSide, .Rows = kSide};
  CHECK(!area.BoundPatch(range, {1, 0}), "reversed terrain intervals are refused");
  CHECK(!area.BoundPatch(range, {0, std::numeric_limits<double>::quiet_NaN()}),
        "unknown terrain cannot become a zero-height bound");
  CHECK(!area.BoundPatch(range, {0, std::numeric_limits<double>::infinity()}),
        "nonfinite terrain cannot admit geometry");
  CHECK(!area.BoundPatch({}, kGroundRangeM), "an empty patch is not a known-empty world");
  CHECK(!area.BoundPatch({.Column = kSide - 1, .Row = 0, .Columns = 2, .Rows = 1}, kGroundRangeM),
        "a patch cannot wrap outside its field");
}
}

int main() {
  const std::array<LongitudeLatitude, 5> sites{{{.LongitudeDeg = 0, .LatitudeDeg = 0},
                                                {.LongitudeDeg = 179.9, .LatitudeDeg = 48},
                                                {.LongitudeDeg = -179.9, .LatitudeDeg = -48},
                                                {.LongitudeDeg = 20, .LatitudeDeg = 84},
                                                {.LongitudeDeg = -20, .LatitudeDeg = -84}}};
  for (const auto site : sites) { EnclosedAt(site); }
  InvalidInputs();
  return Report();
}
