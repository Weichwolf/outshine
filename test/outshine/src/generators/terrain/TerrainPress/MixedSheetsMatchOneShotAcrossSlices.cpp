#include "Check.h"
#include "TerrainPress.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr int kSide = 3;
constexpr int kHalo = 1;
constexpr int kZoom = 20;

outshine::Patchwork Candidate() {
  constexpr uint32_t centre = 1u << (kZoom - 1);
  outshine::Patchwork candidate;
  for (uint32_t offset = 0; offset < 4; ++offset) {
    const size_t nodes = offset == 1 ? 4u : offset == 2 ? 0u : 25u;
    candidate.Sheets.push_back({.Tile = {.Zoom = kZoom, .X = centre + offset, .Y = centre},
                                .Nodes = std::vector<float>(nodes, offset == 1 ? 7.0f : 0.0f),
                                .Side = kSide,
                                .Postings = kSide,
                                .Virtual = true});
  }
  return candidate;
}

outshine::EarthworkStamp StampOf(double halfWidth, double height, outshine::EarthworkKind kind) {
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {
      -halfWidth, -halfWidth, halfWidth, -halfWidth, halfWidth, halfWidth, -halfWidth, halfWidth};
  stamp.LowE = -halfWidth;
  stamp.HighE = halfWidth;
  stamp.LowN = -halfWidth;
  stamp.HighN = halfWidth;
  stamp.PlateauM = height;
  stamp.Kind = kind;
  stamp.Fills = kind != outshine::EarthworkKind::Basin;
  return stamp;
}

bool SameFloors(const outshine::Floors &a, const outshine::Floors &b) {
  return a.Stamps == b.Stamps && a.Unreached == b.Unreached && a.Nodes == b.Nodes &&
         a.Contested == b.Contested && a.AboveM == b.AboveM && a.BelowM == b.BelowM &&
         a.UnfilledM == b.UnfilledM && a.WasAboveM == b.WasAboveM && a.WasBelowM == b.WasBelowM;
}

bool SameProduct(const outshine::Patchwork &a, const outshine::Patchwork &b) {
  if (a.Sheets.size() != b.Sheets.size()) { return false; }
  for (size_t i = 0; i < a.Sheets.size(); ++i) {
    if (a.Sheets[i].Nodes != b.Sheets[i].Nodes) { return false; }
  }
  return true;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  const Patchwork source = Candidate();
  std::vector<EarthworkStamp> stamps;
  stamps.push_back(StampOf(100.0, 5.0, EarthworkKind::Pad));
  stamps.push_back(StampOf(70.0, 2.0, EarthworkKind::Corridor));
  stamps.back().SlopeE = 0.03;
  stamps.push_back(StampOf(15.0, -3.0, EarthworkKind::Basin));

  Patchwork oneShot = source;
  const PressedTerrain expected =
      PressTerrain(stamps, oneShot, TangentFrame::At({}), {.Side = kSide, .Halo = kHalo}, 30.0);
  CHECK(expected.Nodes > 0 && expected.Pads.Stamps == 1 && expected.Corridors.Stamps == 1,
        "overlapping pad, corridor and basin exercise valid sheets");
  CHECK(oneShot.Sheets[1].Nodes == source.Sheets[1].Nodes &&
            oneShot.Sheets[2].Nodes == source.Sheets[2].Nodes,
        "one-shot pressing skips malformed and empty sheets");

  for (const size_t slice : {1u, 3u, 11u, 128u}) {
    Patchwork candidate = source;
    TerrainPressJob job(
        stamps, candidate, TangentFrame::At({}), {.Side = kSide, .Halo = kHalo}, 30.0);
    bool complete = false;
    for (int step = 0; step < 1000 && !complete; ++step) { complete = job.Advance(1, slice); }
    CHECK(complete, "every interruption size completes the same candidate");
    if (!complete) { continue; }
    const PressedTerrain actual = job.Take();
    CHECK(SameProduct(candidate, oneShot) && actual.Nodes == expected.Nodes &&
              actual.Structures == expected.Structures && actual.Held == expected.Held &&
              actual.DeepestM == expected.DeepestM && actual.RaisedM == expected.RaisedM &&
              SameFloors(actual.Pads, expected.Pads) &&
              SameFloors(actual.Corridors, expected.Corridors),
          "mixed stamps and skipped sheets keep byte-identical heights and floor diagnostics");
  }

  Patchwork published = source;
  {
    Patchwork cancelled = source;
    TerrainPressJob job(
        stamps, cancelled, TangentFrame::At({}), {.Side = kSide, .Halo = kHalo}, 30.0);
    for (int step = 0; step < 200; ++step) { (void)job.Advance(1, 1); }
    CHECK(!SameProduct(cancelled, source) && SameProduct(published, source),
          "a partially written private candidate cannot alter publication");
  }
  CHECK(SameProduct(published, source), "discarding an unfinished job leaves publication intact");

  Patchwork invalid = source;
  TerrainPressJob invalidJob(stamps,
                             invalid,
                             TangentFrame::At({}),
                             {.Side = std::numeric_limits<int>::max(), .Halo = kHalo},
                             30.0);
  CHECK(invalidJob.Advance(1, 1) && invalidJob.Take().Nodes == 0 && SameProduct(invalid, source),
        "an invalid layout finishes without touching any candidate sheet");

  Patchwork unstamped = source;
  TerrainPressJob emptyJob(
      {}, unstamped, TangentFrame::At({}), {.Side = kSide, .Halo = kHalo}, 30.0);
  CHECK(emptyJob.Advance(1, 1) && emptyJob.Take().Nodes == 0 && SameProduct(unstamped, source),
        "empty stamp input finishes without rewriting terrain");
  return Report();
}
