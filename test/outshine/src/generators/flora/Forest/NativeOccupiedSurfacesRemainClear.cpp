#include "Check.h"
#include "Forest.h"
#include "GroundSample.h"
#include "RegionPool.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const Tile region = Tile::Of(18, {.LongitudeDeg = 10, .LatitudeDeg = 47});
  std::array<GroundPatch::Posting, 4> samples;
  for (auto &sample : samples) { sample.Height = GroundSample::At(0); }
  const auto frame = TangentFrame::At(region.Geo({0, 0}));
  auto grid = std::make_shared<ClassStructure::Grid>();
  grid->W = grid->H = 1;
  grid->OrgE = grid->OrgN = -1;
  grid->CellM = 2 * std::max(region.SpanEm(), region.SpanNm()) + 2;
  grid->Cells = {1u << 8, 0};
  Generators::Ground::Snapshot snapshot;
  snapshot.Patch = GroundPatch::Complete(region, 2, samples);
  snapshot.Classes = std::make_shared<ClassStructure>(
      frame, grid, std::make_shared<const ClassStructure::Grid>(), ClassStructure::FromRun{});
  const std::array<GroundTable::Row, 1> rows{};
  snapshot.Table = GroundTable::Of(rows);
  const std::array<float, 1> density{1};
  const Forest forest(Forest::Stem{}, density, AlpineLimit{});
  const float east = static_cast<float>(region.SpanEm());
  const float north = static_cast<float>(region.SpanNm());
  for (const int variant : {0, 1, 2, 3, 4}) {
    const float width = variant == 4 ? east / 2 : east;
    std::vector<FeatureField::Vertex> vertices{{0, 0}, {width, 0}, {width, north}, {0, north}};
    const std::array<FeatureField::Ring, 1> rings{{{.First = 0, .Count = 4}}};
    FeatureKind kind = FeatureKind::Structure;
    if (variant == 2) { kind = FeatureKind::Water; }
    if (variant == 3) { kind = FeatureKind::Way; }
    const std::array<FeatureField::Feature, 1> features{
        {{.FirstRing = 0, .RingCount = 1, .CoverRow = 0, .Kind = kind, .Form = FeatureForm::Area}}};
    if (variant == 0) {
      snapshot.Features = FeatureField::Of({}, {}, {});
    } else if (variant == 3) {
      vertices = {{east / 2, 0}, {east / 2, north}};
      const std::array<FeatureField::Ring, 1> line{{{.First = 0, .Count = 2}}};
      auto ribbon = features;
      ribbon[0].Form = FeatureForm::Ribbon;
      ribbon[0].HalfWidthM = east;
      snapshot.Features = FeatureField::Of(ribbon, line, vertices);
    } else {
      snapshot.Features = FeatureField::Of(features, rings, vertices);
    }
    const auto ground = Generators::Ground::Of(region, snapshot);
    CHECK(ground.has_value(), "the native terrain and occupied footprint fixture is complete");
    if (!ground) { continue; }
    RegionPool pool({.Reached = region, .Anywhere = region}, {});
    auto lease = pool.TryAcquire(*ground);
    CHECK(lease.has_value(), "the fixture obtains a production placement sink");
    if (!lease) { continue; }
    std::vector<Yield::Note> notes(forest.NoteNames().size());
    Yield yield(lease->Sink(), forest.NoteNames(), notes);
    forest.Occupy(*ground, yield);
    const auto bodies = lease->Sink().Placed();
    if (variant == 0 || variant == 4) {
      CHECK(!bodies.empty(), "free fertile ground retains vegetation");
    } else {
      CHECK(bodies.empty(), "native building, water and road footprints remain clear of trees");
    }
    if (variant == 4) {
      bool clear = true;
      for (const Solid &body : bodies) { clear = clear && body.Em > width; }
      CHECK(clear, "a partial footprint excludes only its occupied portion of the tile");
    }
  }
  return Report();
}
