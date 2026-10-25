#include "BuildingFoundationFixture.h"
#include "BuildingFoundationJob.h"
#include "Check.h"
#include "Geodesy.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {
constexpr double kCeilingAslM = 125;
constexpr auto kShiftDeg = 0.01;
constexpr auto kRevision = 17;
constexpr auto kToleranceM = 1e-6;
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array points{49.2, 8.3, 49.2, 8.301, 49.201, 8.301, 49.201, 8.3, 49.2004, 8.3003};
  const Ground::BuildingFootprint print{
      .PointCount = 4,
      .SeatM = 20,
      .Street = {},
      .TerrainCeiling =
          Ground::BuildingTerrainCeiling{.OriginPoint = 4, .HeightAslM = kCeilingAslM},
      .TerrainContacts = {}};
  const TangentFrame frame = TangentFrame::At({.LongitudeDeg = 8, .LatitudeDeg = 48});
  const std::array footprints{print};
  BuildingFoundationJob job(
      frame, kRevision, ::outshine::Test::FoundationProducts(footprints, points, {}));
  bool done = false;
  for (size_t step = 0; step < 32 && !done; ++step) {
    const auto advance = job.Advance({.Generation = kRevision, .UnitsMost = 1});
    CHECK(advance.has_value(), "bounded stamp work accepts cached contact metadata");
    if (!advance) { return Report(); }
    done = *advance;
  }
  const auto stamps = job.Take();
  CHECK(done && stamps && stamps->size() == 1, "interrupted work publishes one valid contact");
  if (!stamps || stamps->size() != 1) { return Report(); }
  const auto &stamp = stamps->front();
  const LongitudeLatitudeHeight reference{
      .LongitudeDeg = points[9], .LatitudeDeg = points[8], .HeightM = 125};
  const auto normal = EnuAxesEcef(reference).Up;
  Vec3 origin;
  GeoToEcef(reference, origin);
  for (size_t point = 0; point < 4; ++point) {
    const EastNorth at{.EastM = stamp.RingEastNorthM[point * 2],
                       .NorthM = stamp.RingEastNorthM[point * 2 + 1]};
    const Vec3 target =
        frame.ToEcefPosition({.EastM = at.EastM, .NorthM = at.NorthM, .UpM = stamp.WantsAt(at)});
    CHECK(std::abs(Dot(target - origin, normal)) < kToleranceM,
          "every contact target lies on the stored physical source plane");
  }
  CHECK(std::abs(stamp.SlopeN) > kShiftDeg,
        "a distant tangent frame cannot flatten the source plane into a local horizontal pad");
  const auto first = frame.ToLocalPosition(
      {.LongitudeDeg = points[1], .LatitudeDeg = points[0], .HeightM = print.SeatM});
  CHECK(std::abs(stamp.WantsAt({.EastM = first.EastM, .NorthM = first.NorthM}) - first.UpM) > 100,
        "the cached plane replaces the independent mean height");
  auto invalid = footprints;
  invalid[0].TerrainCeiling =
      Ground::BuildingTerrainCeiling{.OriginPoint = UINT32_MAX, .HeightAslM = kCeilingAslM};
  BuildingFoundationJob bad(
      frame, kRevision, ::outshine::Test::FoundationProducts(invalid, points, {}));
  CHECK(!bad.Advance({.Generation = kRevision, .UnitsMost = 32}),
        "invalid cached origin references fail before coordinate access");
  return Report();
}
