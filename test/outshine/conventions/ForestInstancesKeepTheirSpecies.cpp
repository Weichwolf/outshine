#include <array>
#include <memory>
#include <vector>
#include "Check.h"
#include "ForestDraw.h"
#include "GroundSample.h"
#include "BuildingDraw.h"
#include "WorldPlacement.h"
#include "Shipped.h"
#include "Species.h"
#include "RegionPool.h"

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
  Generators::Ground::Snapshot snapshot;
  snapshot.Patch = GroundPatch::Complete(region, 2, samples);
  snapshot.Classes = std::make_shared<ClassStructure>(
      TangentFrame::At({.LongitudeDeg = 10, .LatitudeDeg = 47}),
      std::make_shared<ClassStructure::Grid>(), std::make_shared<ClassStructure::Grid>(),
      ClassStructure::FromRun{});
  snapshot.Features = FeatureField::Of({}, {}, {});
  const std::array<GroundTable::Row, 1> rows{};
  snapshot.Table = GroundTable::Of(rows);
  const auto ground = Generators::Ground::Of(region, snapshot);
  CHECK(ground.has_value(), "the draw fixture has a valid ground snapshot");
  if (!ground) { return Report(); }
  outshine::Ground::GroundMaterials materials;
  outshine::Ground::VegetationTemplates vegetation;
  std::string error;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "shipped ground materials load");
  CHECK(vegetation.Load("src/assets/world/vegetation.json",materials), "shipped vegetation loads");
  std::vector<TreeSpecies> sources;
  CHECK(ReadSpecies("src/assets/world/species",sources,error) && !sources.empty(), "source species catalogue loads");
  Shipping shipping;
  CHECK(shipping.TreeFor(ClusterId{0}) == nullptr, "an unprepared catalogue resolves no tree");
  CHECK(shipping.Stands(vegetation,"src/assets/world/species",error), "world catalogue stands");
  CHECK(shipping.TreeFor(ClusterId{static_cast<uint32_t>(sources.size())}) == nullptr &&
        shipping.TreeFor(ClusterId{~0u}) == nullptr, "building and unknown clusters do not resolve as trees");
  for (size_t index=0; index<sources.size(); ++index) {
    const auto *held=shipping.TreeFor(ClusterId{static_cast<uint32_t>(index)});
    CHECK(held && held->Name()==sources[index].Name() && held->HeightM()==sources[index].HeightM(),
          "each cluster retains its source species and prototype height");
  }
  const auto *first=shipping.TreeFor(ClusterId{0});
  CHECK(shipping.Stands(vegetation,"src/assets/world/species",error) && shipping.TreeFor(ClusterId{0})==first,
        "repeated preparation preserves the immutable prototype address");
  RegionPool pool({.Reached=region,.Anywhere=region},{});
  auto lease=pool.TryAcquire(*ground);
  CHECK(lease.has_value(), "catalogue fixture leases a real placement sink");
  if (lease) {
    const auto &makers=shipping.Placing();
    std::vector<std::vector<Yield::Note>> notes(makers.Count());
    std::vector<Yield> yields;
    for (size_t maker=0; maker<makers.Count(); ++maker) {
      const auto names=makers.At(maker).NoteNames();
      notes[maker].resize(names.size());
      yields.emplace_back(lease->Sink(),names,notes[maker]);
      if (makers.At(maker).Called() != "flora") { continue; }
      for (size_t index=0; index<sources.size(); ++index) {
        Solid body;
        body.Em=10+index*8;
        body.Nm=10;
        body.BaseAslM=500;
        body.HeightM=sources[index].HeightM()*1.5f;
        body.RadiusM=0.1f;
        body.Variant=static_cast<uint32_t>(index);
        CHECK(yields.back().Place(body).Why()==Claim::Outcome::Placed, "each species occupies a distinct fixture position");
      }
    }
    Captured shipped;
    shipping.Drawing().Draw(*ground,makers,yields,lease->Sink().Placed(),shipped);
    CHECK(shipped.Instances.size()==sources.size(), "the shipped draw registry reaches every placed species");
    for (const auto &instance : shipped.Instances) {
      const auto *held=shipping.TreeFor(instance.Cluster);
      const auto &body=lease->Sink().Placed()[instance.Body];
      CHECK(held && body.Variant<sources.size() && held->Name()==sources[body.Variant].Name(),
            "world instance cluster resolves back to its placed source species");
      CHECK_NEAR(instance.Where.Scale,1.5,1e-6,"ratio", "catalogue drawing scales relative to its own retained source");
    }
  }
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
