#include <array>
#include <memory>
#include <vector>
#include "Check.h"
#include "ForestDraw.h"
#include "GroundSample.h"
#include "BuildingDraw.h"
#include "WorldPlacement.h"

namespace {
class Captured final : public outshine::Generators::DrawSink {
public:
  struct Instance {
    uint32_t Body;
    outshine::Generators::ClusterId Cluster;
    outshine::Generators::Scattered Where;
  };
  std::vector<Instance> Instances;
  bool Add(outshine::Generators::BodyId body,
           outshine::Generators::ClusterId cluster,
           const outshine::Generators::Scattered &where) noexcept override {
    Instances.push_back({body.Index(), cluster, where});
    return true;
  }
  bool Full() const noexcept override { return false; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const Tile region = Tile::Of(14, {.LongitudeDeg = 10, .LatitudeDeg = 47});
  std::array<GroundPatch::Posting, 4> samples;
  for (auto &sample : samples) { sample.Height = GroundSample::At(500); }
  Ground::Snapshot snapshot;
  snapshot.Patch = GroundPatch::Complete(region, 2, samples);
  snapshot.Classes = std::make_shared<ClassStructure>(
      TangentFrame::At({.LongitudeDeg = 10, .LatitudeDeg = 47}),
      std::make_shared<ClassStructure::Grid>(), std::make_shared<ClassStructure::Grid>(),
      ClassStructure::FromRun{});
  snapshot.Features = FeatureField::Of({}, {}, {});
  const std::array<GroundTable::Row, 1> rows{};
  snapshot.Table = GroundTable::Of(rows);
  const auto ground = Ground::Of(region, snapshot);
  CHECK(ground.has_value(), "the draw fixture has a valid ground snapshot");
  if (!ground) { return Report(); }
  const std::array<ForestDraw::Prototype, 2> prototypes = {{
      {.Cluster = ClusterId{17}, .HeightM = 10},
      {.Cluster = ClusterId{29}, .HeightM = 30}}};
  const ForestDraw draw(prototypes);
  std::array<Solid, 2> bodies = {{
      {.Em = 12 + 0x1p-30, .Nm = 23 + 0x1p-30, .BaseAslM = 501 + 0x1p-30, .HeightM = 15, .YawRad = 0.4f, .Variant = 0},
      {.Em = 34, .Nm = 45, .BaseAslM = 502, .HeightM = 45, .YawRad = 0.7f, .Variant = 1}}};
  Captured captured;
  draw.Draw(*ground, bodies, {.First = 11, .Count = 2}, captured);
  CHECK(captured.Instances.size() == 2, "both species reach the instance sink");
  if (captured.Instances.size() != 2) { return Report(); }
  for (size_t at = 0; at < bodies.size(); ++at) {
    const auto &instance = captured.Instances[at];
    CHECK(instance.Cluster == prototypes[at].Cluster, "a body keeps its own species prototype");
    CHECK(instance.Body == 11 + at, "the generator preserves the allocated body range");
    CHECK_NEAR(instance.Where.Scale, 1.5, 1e-6, "ratio", "scale is relative to this species, not the first tree");
    CHECK(instance.Where.Em == bodies[at].Em, "east retains the source double precision in the region frame");
    CHECK(instance.Where.Nm == bodies[at].Nm, "north retains the source double precision in the region frame");
    CHECK(instance.Where.AslM == bodies[at].BaseAslM, "height retains its datum and source double precision");
  }
  bodies[1].Variant = 0;
  Captured buildings;
  BuildingDraw(ClusterId{31}, 10).Draw(*ground, bodies, {.First = 11, .Count = 2}, buildings);
  CHECK(buildings.Instances.size() == bodies.size(), "building placements also reach the sink");
  if (buildings.Instances.size() == bodies.size()) {
    for (size_t at = 0; at < bodies.size(); ++at) {
      const auto &where = buildings.Instances[at].Where;
      CHECK(where.Em == bodies[at].Em && where.Nm == bodies[at].Nm && where.AslM == bodies[at].BaseAslM,
            "building positions retain source double precision");
    }
  }
  const Tile adjacent(region.Zoom(), region.X() + 1, region.Y());
  const LongitudeLatitude target{.LongitudeDeg = 10, .LatitudeDeg = 47};
  std::array<WorldPlacement,2> world;
  size_t at = 0;
  for (const Tile &tile : {region, adjacent}) {
    const EastNorth local = tile.Enu(target);
    Scattered placed = captured.Instances.front().Where;
    placed.Em = local.EastM;
    placed.Nm = local.NorthM;
    world[at] = WorldPlacement::From(tile, placed);
    CHECK_NEAR(world[at].Position.LongitudeDeg, target.LongitudeDeg, 1e-12, "deg", "region-local east reaches geographic longitude");
    CHECK_NEAR(world[at].Position.LatitudeDeg, target.LatitudeDeg, 1e-12, "deg", "region-local north reaches geographic latitude");
    CHECK(world[at].AslM == placed.AslM && world[at].YawRad == placed.YawRad && world[at].Scale == placed.Scale,
          "geographic handoff preserves ASL datum, orientation and scale");
    ++at;
  }
  CHECK_NEAR(world[0].Position.LongitudeDeg, world[1].Position.LongitudeDeg, 1e-12, "deg", "different region frames agree at the same world position");
  Captured changed;
  draw.Draw(*ground, bodies, {.First = 11, .Count = 2}, changed);
  CHECK(changed.Instances[1].Cluster != prototypes[1].Cluster,
        "negative control: erasing the species changes the referenced prototype");
  CHECK(changed.Instances[1].Where.Scale != captured.Instances[1].Where.Scale,
        "negative control: erasing the species also corrupts its scale");
  return Report();
}
