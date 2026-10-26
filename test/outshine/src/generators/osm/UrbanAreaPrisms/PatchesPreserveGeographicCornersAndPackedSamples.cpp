#include "Check.h"
#include "UrbanAreaPrisms.h"
#include "Geodesy.h"
#include "Units.h"
#include "Vec3.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstddef>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = Generators::Osm;
constexpr size_t kSide = 8;
constexpr double kToleranceM = 1e-8;

void CheckAt(LongitudeLatitude at) {
  Osm::UrbanArea area;
  area.Columns = area.Rows = kSide;
  const double pitch = area.Recipe.ProjectedPitchM;
  area.FirstCell = {static_cast<int64_t>(std::floor(kWgs84A * at.LongitudeDeg * kDeg2Rad / pitch)),
                    static_cast<int64_t>(std::floor(
                        kWgs84A * std::asinh(std::tan(at.LatitudeDeg * kDeg2Rad)) / pitch))};
  area.Tiers.assign(kSide * kSide, 4);
  area.Tiers[1] = 0;
  std::array<std::optional<double>, kSide * kSide> ground;
  for (size_t i = 0; i < ground.size(); ++i) { ground[i] = 1200 + static_cast<double>(i); }
  ground[1].reset();
  const std::array inputs{
      Osm::UrbanPrismInput{.Range = {.Columns = kSide, .Rows = kSide},
                           .GroundAslM = Osm::UrbanGroundHeights{ground.data(), kSide, kSide}},
      Osm::UrbanPrismInput{.Range = {.Columns = 1, .Rows = 1},
                           .GroundAslM = Osm::UrbanGroundHeights{ground.data(), 1, 1}}};
  const auto prepared = Osm::PrepareUrbanPrisms(area, inputs, kSide * kSide + 1);
  CHECK(prepared && prepared->Geometry.Patches.size() == 2 && prepared->Placements.size() == 2,
        "independent patch frames share a single immutable sample allocation");
  if (!prepared) { return; }
  CHECK(prepared->Geometry.Samples->size() == kSide * kSide + 1 &&
            prepared->Geometry.Patches[1].FirstSample == kSide * kSide,
        "sample ranges remain packed and only selected cells consume storage");
  CHECK((*prepared->Geometry.Samples)[1].Upper == (*prepared->Geometry.Samples)[1].Lower,
        "empty cells preserve their grid position without requiring terrain");
  for (size_t patchIndex = 0; patchIndex < inputs.size(); ++patchIndex) {
    const auto &shape = prepared->Geometry.Patches[patchIndex];
    const auto &placement = prepared->Placements[patchIndex];
    double measured = 0;
    for (uint32_t row = 0; row < shape.Rows; ++row) {
      for (uint32_t column = 0; column < shape.Columns; ++column) {
        const auto lot = area.LotAt(column, row);
        if (!lot) { continue; }
        Vec3 base;
        const LongitudeLatitudeHeight geo{.LongitudeDeg = Wrap180(lot->Centre.LongitudeDeg),
                                          .LatitudeDeg = lot->Centre.LatitudeDeg,
                                          .HeightM = *inputs[patchIndex].GroundAslM[row, column]};
        GeoToEcef(geo, base);
        const auto axes = EnuAxesEcef(geo);
        const auto sample =
            (*prepared->Geometry.Samples)[shape.FirstSample + row * shape.Columns + column];
        for (const double x : {-.5, .5}) {
          for (const double z : {-.5, .5}) {
            for (const bool top : {false, true}) {
              const Vec3 reference = base + axes.East * (x * lot->EastSpanM) -
                                     axes.North * (z * lot->NorthSpanM) +
                                     axes.Up * (top ? lot->HeightM : 0.);
              const Vec3 local{{(column + .5 + x * shape.FillFraction) * shape.PitchX,
                                top ? sample.Upper : sample.Lower,
                                (row + .5 + z * shape.FillFraction) * shape.PitchZ}};
              measured = std::max(
                  measured, Length(placement.EcefFromLocal.TransformPoint(local) - reference));
            }
          }
        }
      }
    }
    CHECK(std::abs(measured - placement.MaxCornerErrorM) < kToleranceM,
          "the retained numerical deviation includes all corners and float sample parameters");
    CHECK(measured < .1, "small local frames stay close to upright curved-earth lots");
  }
}
}

int main() {
  for (const auto at :
       std::array<LongitudeLatitude, 5>{{{.LongitudeDeg = 0, .LatitudeDeg = 0},
                                         {.LongitudeDeg = 179.99999, .LatitudeDeg = 48},
                                         {.LongitudeDeg = -179.99999, .LatitudeDeg = -48},
                                         {.LongitudeDeg = 25, .LatitudeDeg = 84},
                                         {.LongitudeDeg = -140, .LatitudeDeg = -84}}}) {
    CheckAt(at);
  }
  return Report();
}
