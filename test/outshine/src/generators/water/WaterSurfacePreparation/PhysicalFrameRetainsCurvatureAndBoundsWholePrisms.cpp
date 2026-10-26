#include "WaterPhysicalFrame.h"
#include "Check.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckHeightTargets() {
  const std::array<LongitudeLatitude, 4> anchors{
      {{0, 0}, {179.999, 80}, {-179.999, -80}, {90, 90}}};
  const std::array<LongitudeLatitudeHeight, 4> points{
      {{.LongitudeDeg = .01, .LatitudeDeg = -.02, .HeightM = -500},
       {.LongitudeDeg = -179.98, .LatitudeDeg = 80.01, .HeightM = 1300},
       {.LongitudeDeg = 179.98, .LatitudeDeg = -80.01, .HeightM = 0},
       {.LongitudeDeg = -120, .LatitudeDeg = 89.99, .HeightM = 20}}};
  for (size_t at = 0; at < anchors.size(); ++at) {
    const auto frame = TangentFrame::At(anchors[at]);
    const auto local = frame.ToLocalPosition(points[at]);
    const Vec3 position{{local.EastM, local.NorthM, local.UpM}};
    const auto restored = GeodeticWaterPoint(frame, position);
    const auto target = WaterHeightTarget(
        frame, {.EastM = local.EastM, .NorthM = local.NorthM}, points[at].HeightM);
    CHECK(restored && std::abs(restored->Geographic.HeightM - points[at].HeightM) < 2e-8,
          "independent library inversion recovers forward-placed signed model heights");
    CHECK(target && std::abs(*target - local.UpM) < 2e-8,
          "fixed horizontal coordinates intersect the declared height across poles and dateline");
    const auto up = frame.ToLocalDirection(EnuAxesEcef(points[at]).Up);
    CHECK(restored && Length(restored->Up - Vec3{{up.EastM, up.NorthM, up.UpM}}) < 1e-13,
          "physical normals use the geographic point rather than the anchor vertical");
  }
  constexpr double east = 50000;
  const auto sag = WaterHeightTarget(TangentFrame::At({}), {.EastM = east}, 0);
  const double exact = std::sqrt(kWgs84A * kWgs84A - east * east) - kWgs84A;
  CHECK(sag && *sag < -100 && std::abs(*sag - exact) < 2e-8,
        "sea-level model targets retain the analytic equatorial curvature sag");
}

void CheckPrismBounds() {
  const std::array<std::array<double, 2>, 3> triangle{{{-200, -100}, {200, -100}, {30, 250}}};
  const std::array<std::array<double, 2>, 3> prisms{{{-900, -800}, {-2, 2}, {430, 490}}};
  const auto frame = TangentFrame::At({.LongitudeDeg = 179.999, .LatitudeDeg = 75});
  constexpr int samples = 8;
  for (const auto bounds : prisms) {
    const auto cap = BoundWaterFacetNormals(frame, triangle, bounds);
    CHECK(cap, "positive and signed prisms have finite whole-domain normal caps");
    if (!cap) { continue; }
    for (int x = 0; x <= samples; ++x) {
      for (int y = 0; y <= samples - x; ++y) {
        const double a = static_cast<double>(x) / samples;
        const double b = static_cast<double>(y) / samples;
        const double east = triangle[0][0] * (1 - a - b) + triangle[1][0] * a + triangle[2][0] * b;
        const double north = triangle[0][1] * (1 - a - b) + triangle[1][1] * a + triangle[2][1] * b;
        for (int level = 0; level <= 2; ++level) {
          const double height = bounds[0] + (bounds[1] - bounds[0]) * level / 2;
          const auto ecef = frame.ToEcefPosition({.EastM = east, .NorthM = north, .UpM = height});
          const auto geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
          const auto up = frame.ToLocalDirection(
              EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}).Up);
          CHECK(Length(Vec3{{up.EastM, up.NorthM, up.UpM}} - cap->Up) <= cap->Delta,
                "independent inverse normals throughout the prism stay inside the bound");
        }
      }
    }
  }
}

void CheckInvalidDomains() {
  const auto frame = TangentFrame::At({});
  const auto nonfinite = WaterHeightTarget(frame, {}, std::numeric_limits<double>::infinity());
  CHECK(!nonfinite && nonfinite.error() == WaterFrameError::InvalidInput,
        "nonfinite model heights never enter geodetic inversion");
  const auto horizon = WaterHeightTarget(frame, {.EastM = kWgs84A}, 0);
  CHECK(!horizon && horizon.error() == WaterFrameError::OutsideLocalDomain,
        "a height target beyond the declared local graph has no fabricated answer");
  const std::array<std::array<double, 2>, 3> small{{{0, 0}, {20, 0}, {0, 20}}};
  const auto singular = BoundWaterFacetNormals(frame, small, {-kWgs84A, 0});
  CHECK(!singular && singular.error() == WaterFrameError::InvalidInput,
        "prisms reaching the geodetic normal singularity are rejected");
  CHECK(!BoundWaterFacetNormals(frame, small, {10, 0}) &&
            !BoundWaterFacetNormals(frame, small, {0, 0}),
        "normal bounds require an ordered nonempty height interval");
  const std::array<std::array<double, 2>, 3> broad{{{0, 0}, {1000000, 0}, {0, 1000000}}};
  const auto outside = BoundWaterFacetNormals(frame, broad, {0, 20});
  CHECK(!outside && outside.error() == WaterFrameError::OutsideLocalDomain,
        "a broad facet cannot silently exceed the proven normal-cap domain");
}
}

int main() {
  CheckHeightTargets();
  CheckPrismBounds();
  CheckInvalidDomains();
  return Report();
}
