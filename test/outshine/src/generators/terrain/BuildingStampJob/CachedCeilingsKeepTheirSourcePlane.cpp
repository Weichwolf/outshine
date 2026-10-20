#include "BuildingStampJob.h"
#include "Check.h"
#include "Geodesy.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array points{49.2, 8.3, 49.2, 8.301, 49.201, 8.301, 49.201, 8.3, 49.2004, 8.3003};
  const Ground::BuildingFootprint print{
      .PointCount = 4,
      .SeatM = 20,
      .TerrainCeiling = Ground::BuildingTerrainCeiling{.OriginPoint = 4, .HeightAslM = 125}};
  const TangentFrame frame = TangentFrame::At({.LongitudeDeg = 8, .LatitudeDeg = 48});
  const std::array footprints{print};
  BuildingStampJob job(frame, 17);
  bool done = false;
  for (size_t step = 0; step < 32 && !done; ++step) {
    const auto advance = job.Advance(
        {.Footprints = footprints, .Points = points, .VectorGeneration = 17, .UnitsMost = 1});
    CHECK(advance.has_value(), "bounded stamp work accepts cached contact metadata");
    if (!advance) { return Report(); }
    done = *advance;
  }
  const auto stamps = std::move(job).Take();
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
    CHECK(std::abs(Dot(target - origin, normal)) < 1e-6,
          "every contact target lies on the stored physical source plane");
  }
  CHECK(std::abs(stamp.SlopeN) > 0.01,
        "a distant tangent frame cannot flatten the source plane into a local horizontal pad");
  const auto first = frame.ToLocalPosition(
      {.LongitudeDeg = points[1], .LatitudeDeg = points[0], .HeightM = print.SeatM});
  CHECK(std::abs(stamp.WantsAt({.EastM = first.EastM, .NorthM = first.NorthM}) - first.UpM) > 100,
        "the cached plane replaces the independent mean height");
  auto invalid = footprints;
  invalid[0].TerrainCeiling->OriginPoint = UINT32_MAX;
  BuildingStampJob bad(frame, 17);
  CHECK(!bad.Advance(
            {.Footprints = invalid, .Points = points, .VectorGeneration = 17, .UnitsMost = 32}),
        "invalid cached origin references fail before coordinate access");
  return Report();
}
