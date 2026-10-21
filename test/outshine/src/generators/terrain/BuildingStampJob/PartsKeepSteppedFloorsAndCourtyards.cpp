#include "BuildingStampJob.h"
#include "Check.h"
#include "Geodesy.h"
#include <array>
#include <cmath>
#include <memory>
#include <vector>

using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

int main() {
  const std::array points{47.0, 8.0, 47.0, 8.001, 47.001, 8.001, 47.001, 8.0};
  const std::vector<Ground::BuildingTerrainPart> parts{
      {.HeightAslM = 100,
       .Ring = {{0, 0}, {10, 0}, {10, 10}, {0, 10}},
       .Holes = {{{2, 2}, {4, 2}, {4, 4}, {2, 4}}}},
      {.HeightAslM = 105, .Ring = {{10, 0}, {20, 0}, {20, 10}, {10, 10}}}};
  const std::array footprints{Ground::BuildingFootprint{
      .PointCount = 4,
      .HeightM = 20,
      .BaseM = 80,
      .SeatM = 90,
      .TerrainCeiling = Ground::BuildingTerrainCeiling{.OriginPoint = 0, .HeightAslM = 100},
      .TerrainContacts = {
          .Parts = std::make_shared<const std::vector<Ground::BuildingTerrainPart>>(parts)}}};
  const auto frame = TangentFrame::At({.LongitudeDeg = 9, .LatitudeDeg = 48});
  BuildingStampJob job(frame, 19);
  bool complete = false;
  size_t calls = 0;
  while (!complete && calls < 100) {
    const auto advanced = job.Advance({.Footprints = footprints,
                                       .Points = points,
                                       .Rings = {},
                                       .VectorGeneration = 19,
                                       .UnitsMost = 1});
    CHECK(advanced, "each bounded part-vertex step succeeds");
    if (!advanced) { return Report(); }
    complete = *advanced;
    ++calls;
  }
  const size_t heldBytes = job.HeapBytes();
  const auto stamps = std::move(job).Take();
  CHECK(complete && calls > 20 && stamps && stamps->size() == 2,
        "one footprint emits separate, interruptibly prepared ground contacts");
  if (!stamps || stamps->size() != 2) { return Report(); }
  const auto source = TangentFrame::At({.LongitudeDeg = 8, .LatitudeDeg = 47});
  for (size_t index = 0; index < parts.size(); ++index) {
    const auto &stamp = (*stamps)[index];
    CHECK(stamp.RingEastNorthM.size() == 8 && !stamp.Fills,
          "each piece keeps its physical boundary and existing cut-only policy");
    for (const auto &point : parts[index].Ring) {
      const auto reference = source.ToEcefPosition(
          {.EastM = point.EastM, .NorthM = point.NorthM, .UpM = parts[index].HeightAslM});
      const auto local = frame.ToLocalPosition(reference);
      CHECK(std::abs(stamp.WantsAt({local.EastM, local.NorthM}) - local.UpM) < 1e-6,
            "a remote tangent frame preserves each distinct physical plane");
    }
  }
  CHECK(stamps->front().HoleRingsEastNorthM.size() == 1 &&
            stamps->front().HoleRingsEastNorthM.front().size() == 8 &&
            stamps->back().HoleRingsEastNorthM.empty(),
        "courtyard ownership stays with its original part");
  CHECK(heldBytes > 0 && job.HeapBytes() == 0 && footprints.front().HeapBytes() > 0,
        "contact storage is visible in retained-memory accounting");
  return Report();
}
