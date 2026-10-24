#include "UrbanArea.h"
#include "Check.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include "Wgs84.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = outshine::Generators::Osm;
constexpr double kPitchM = 32;
constexpr double kFill = .75;
constexpr double kDegrees = 180 / std::numbers::pi;
constexpr double kPositionToleranceDeg = 1e-10;
constexpr double kLengthToleranceM = 1e-4;

LongitudeLatitude Projected(double x, double y) {
  const auto point = Ground::TileFracToGeo(
      {.X = x / Data::kMercatorGirthM + .5, .Y = .5 - y / Data::kMercatorGirthM}, 0);
  return {.LongitudeDeg = point.LongitudeDeg, .LatitudeDeg = point.LatitudeDeg};
}

double Distance(LongitudeLatitude first, LongitudeLatitude second) {
  Vec3 a;
  Vec3 b;
  GeoToEcef({.LongitudeDeg = first.LongitudeDeg, .LatitudeDeg = first.LatitudeDeg}, a);
  GeoToEcef({.LongitudeDeg = second.LongitudeDeg, .LatitudeDeg = second.LatitudeDeg}, b);
  return Length(b - a);
}

void CheckAt(LongitudeLatitude focus) {
  const double x = kWgs84A * focus.LongitudeDeg / kDegrees;
  const double y = kWgs84A * std::asinh(std::tan(focus.LatitudeDeg / kDegrees));
  Osm::UrbanArea area;
  area.Recipe = {.ProjectedPitchM = kPitchM, .FillFraction = kFill};
  area.FirstCell = {static_cast<int64_t>(std::floor(x / kPitchM)),
                    static_cast<int64_t>(std::floor(y / kPitchM))};
  area.Columns = area.Rows = 2;
  area.Tiers = {1, 2, 3, 0};
  for (uint32_t row = 0; row < area.Rows; ++row) {
    for (uint32_t column = 0; column < area.Columns; ++column) {
      const auto lot = area.LotAt(column, row);
      const auto tier = area.Tiers[static_cast<size_t>(row) * area.Columns + column];
      CHECK(lot.has_value() == (tier != 0), "only occupied cells produce lots");
      if (!lot) { continue; }
      const double east = (static_cast<double>(area.FirstCell[0]) + column + .5) * kPitchM;
      const double north = (static_cast<double>(area.FirstCell[1]) - row + .5) * kPitchM;
      const auto expected = Projected(east, north);
      CHECK(std::abs(lot->Centre.LongitudeDeg - expected.LongitudeDeg) < kPositionToleranceDeg &&
                std::abs(lot->Centre.LatitudeDeg - expected.LatitudeDeg) < kPositionToleranceDeg,
            "global cell phase and north-to-south rows preserve geographic position");
      constexpr double half = kPitchM * kFill / 2;
      CHECK(std::abs(lot->EastSpanM - Distance(Projected(east - half, north),
                                               Projected(east + half, north))) < kLengthToleranceM,
            "east span agrees with independent WGS84 endpoint chord");
      CHECK(std::abs(lot->NorthSpanM - Distance(Projected(east, north - half),
                                                Projected(east, north + half))) < kLengthToleranceM,
            "north span preserves ellipsoidal scale instead of sharing the east factor");
      CHECK(lot->HeightM == Osm::UrbanArea::HeightsM[tier],
            "logical height has no camera, DEM or curvature offset");
    }
  }
  const auto northwest = area.LotAt(0, 0);
  const auto east = area.LotAt(1, 0);
  const auto south = area.LotAt(0, 1);
  CHECK(northwest && east && south, "the orientation control has three occupied lots");
  if (northwest && east && south) {
    CHECK(east->Centre.LongitudeDeg > northwest->Centre.LongitudeDeg &&
              south->Centre.LatitudeDeg < northwest->Centre.LatitudeDeg,
          "column and row directions cannot mirror the world");
  }
}
}

int main() {
  const std::array<outshine::LongitudeLatitude, 5> sites{
      {{.LongitudeDeg = 0, .LatitudeDeg = 0},
       {.LongitudeDeg = 179.9, .LatitudeDeg = 48},
       {.LongitudeDeg = -179.9, .LatitudeDeg = -48},
       {.LongitudeDeg = 20, .LatitudeDeg = 84},
       {.LongitudeDeg = -20, .LatitudeDeg = -84}}};
  for (const auto site : sites) { CheckAt(site); }
  return outshine::Test::Report();
}
