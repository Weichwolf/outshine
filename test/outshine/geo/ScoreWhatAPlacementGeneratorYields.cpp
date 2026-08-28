#include <cstdio>
#include <memory>
#include <vector>

#include "AlpineLimit.h"
#include "Check.h"
#include "Forest.h"
#include "Ground.h"
#include "GroundQuery.h"
#include "GroundSnapshot.h"
#include "RegionPool.h"
#include "ClassStructure.h"
#include "TangentFrame.h"
#include "Tile.h"
#include "Yield.h"

// A PLACEMENT GENERATOR YIELDS BODIES, AND NOTHING IN THIS TREE ASKED IT TO.
//
// Unreal's PCG graph outputs POINT DATA and separate spawners turn those points into static-mesh
// instances; the point and the mesh are different representations on purpose, because a forest is
// not one mesh and instancing dies the moment you flatten it into one. RAGE has no PCG to
// compare. Taking Unreal, so outshine has two generator doors and both are correct:
// `Generates` hands back a `Geometry` -- a PART -- and `Making` hands back `Body` placements.
//
// MEASURED BEFORE THIS CASE, and it is what the case exists to end. `Generates` had ONE
// implementer, `Structures`, and it is the only thing the engine's registry holds.
// `Generators::Making` had FOUR -- Forest, Buildings, Water, Infrastructure -- and NO consumer
// outside `src/generators/`. `Ground::Of` had no caller. `Occupy` was called only by
// `GeneratorSet`, which is reached only by `DrawSet`, whose `DrawSink` has zero implementers. So
// five thousand eight hundred lines of generator sat behind a chain whose every arrow is written
// and none of which is walked.
//
// THIS CASE WALKS IT, and it stands in a suite that links no engine source at all -- the geo
// suite's group list holds `src/generators` and nothing of `src/engine`. That it compiles and
// links here is part 3 of board:1948 in miniature: another program takes the generator tier
// without the engine behind it.
//
// The oracle is the density the case declares and owes nothing to our design. A forest asked for
// trees over a flat block at a declared stems-per-square-metre yields SOME, and the same forest
// asked at zero density yields NONE -- so what the number measures is the declaration and not the
// ground.

namespace {

constexpr int kZoom = 14;
constexpr double kLatDeg = 48.1372;
constexpr double kLonDeg = 11.5756;
constexpr double kBaseM = 500.0;
constexpr int kSide = 33;
constexpr uint32_t kSinkCapacity = 4096;

class Flat final : public outshine::GroundQuery {
public:
  [[nodiscard]] outshine::GroundSample At(double, double) const override {
    return outshine::GroundSample::At(kBaseM);
  }
  [[nodiscard]] outshine::GroundSample Resident(double lat, double lon) const override {
    return At(lat, lon);
  }
  [[nodiscard]] double PostM(double) const override { return 30.0; }
  [[nodiscard]] outshine::Ground::GroundBlock BlockAt(int z, long x, long y) const override {
    Nodes_.assign((size_t)kSide * (size_t)kSide, (float)kBaseM);
    return outshine::Ground::GroundBlock::Over(Nodes_.data(), z, x, y, kSide, kSide - 1);
  }

private:
  mutable std::vector<float> Nodes_;
};

[[nodiscard]] uint32_t TreesAt(const outshine::Generators::Ground &over, float perM2) {
  using namespace outshine::Generators;
  Forest::Stem stem;
  stem.HeightM = 20.0;
  stem.TrunkRadiusM = 0.15f;
  const float density[1] = {perM2};
  const outshine::AlpineLimit noLimit;
  const Forest forest(stem, outshine::Span<const float>(density, 1), noLimit);

  RegionPool::Shape shape;
  shape.Sinks = 1;
  shape.BodyCapacity = kSinkCapacity;
  shape.CellM = 8.0;
  RegionPool::Extent extent{over.Where(), over.Where()};
  RegionPool pool(extent, shape);
  std::optional<RegionPool::Lease> lease = pool.TryAcquire(over);
  if (!lease) { return 0; }

  std::vector<Yield::Note> notes(forest.NoteNames().Size());
  Yield yield(lease->Sink(), forest.NoteNames(), outshine::Span<Yield::Note>(notes.data(), notes.size()));
  forest.Occupy(over, yield);
  for (const Yield::Note &note : yield.Notes()) {
    if (note.Times == 0) { continue; }
    std::printf("    %-18s %u\n", note.Name ? note.Name : "?", note.Times);
  }
  return yield.Placed().Count;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Generators;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Ground::GroundMaterials materials;
  outshine::Ground::VegetationTemplates templates;
  if (!materials.Load("src/assets/world/ground-materials.json") ||
      !templates.Load("src/assets/world/vegetation.json", materials)) {
    Unprepared("the shipped ground tables do not load");
    return Report();
  }
  const std::shared_ptr<const GroundTable> table = TableOf(templates);
  if (!table) {
    Unprepared("the ground table would not build from the shipped templates");
    return Report();
  }

  const Tile region = Tile::Of(kZoom, kLatDeg, kLonDeg);
  const Flat flat;
  outshine::Ground::ClassField classes;
  Fields stands;
  Ground::Snapshot snapshot;
  (void)SnapshotOver(region, flat, classes, stands, table, &snapshot);
  if (!snapshot.Patch) {
    Unprepared("the height patch over a flat block was not built");
    return Report();
  }
  // THE CLASS STRUCTURE AND THE FEATURES COME FROM A TILE POOL over the network, and a case that
  // waits for one measures the network. They are built here instead: one class cell with every
  // probe unmapped and no edges, and an EMPTY feature field -- which together are the ground a
  // forest reads where nothing has been classified and nothing has been surveyed, and exactly the
  // case a density declaration must still answer.
  {
    auto grid = std::make_shared<outshine::ClassStructure::Grid>();
    grid->W = 1;
    grid->H = 1;
    grid->CellM = region.SpanEm() > 0.0 ? region.SpanEm() : 1.0;
    grid->Cells.assign(1, 0u);
    grid->Edges.assign(1, (float)outshine::ClassStructure::kNoEdgeM);
    snapshot.Classes = std::make_shared<const outshine::ClassStructure>(
        outshine::TangentFrame::At(kLatDeg, kLonDeg), grid, grid, 1u, 0, 0.0, 0);
    snapshot.Features = FeatureField::Of(outshine::Span<const FeatureField::Feature>(),
                                         outshine::Span<const FeatureField::Ring>(),
                                         outshine::Span<const FeatureField::Vertex>());
    snapshot.Table = table;
  }
  if (!snapshot.Classes || !snapshot.Features || !snapshot.Table) {
    Unprepared("the snapshot would not stand up from a hand-built class structure");
    return Report();
  }
  const std::optional<Ground> over = Ground::Of(region, snapshot);
  if (!over) {
    Unprepared("Ground::Of refused a snapshot that was taken -- the chain breaks before the "
               "generator is asked anything");
    return Report();
  }

  // THE DENSITY IS CHOSEN SO THE SINK DOES NOT SATURATE. At 0.02 stems/m2 over a zoom-14 tile
  // this yields exactly 4096 -- the sink's own capacity -- and a number that reads its own cap
  // measures the cap rather than the declaration. 4.0e-4 lands well under it.
  //
  // The count is NOT the area times the density, and the notes the run prints say why. Measured
  // and checked, all of it from this case's own output:
  //
  //   noTemplate        197769   cells the unmapped class structure gives no vegetation template
  //   densityDraw        42140   cells that had one and lost the draw
  //   placed               191
  //   total             240100 = 490 x 490 cells, so the tile spans 490 x 3.33 m = 1632 m
  //
  //   p per cell, measured   191 / 42331          = 4.512e-3
  //   p per cell, derived    4.0e-4 x 3.33 x 3.33 = 4.436e-3   -- agree to 1.7%
  //
  // So `Forest` walks a 3.33 m lattice and draws ONCE PER CELL: the declared density is a
  // probability per cell area, not a count per square metre, and it applies only where a template
  // stands. A first version of this comment predicted area times density and was wrong by twelve;
  // the notes are printed instead, because a number a reader cannot decompose cannot be checked.
  constexpr float kStemsPerM2 = 4.0e-4f;
  const uint32_t dense = TreesAt(*over, kStemsPerM2);
  const uint32_t none = TreesAt(*over, 0.0f);

  std::printf("A FOREST AT %.1e stems/m2  yields %u bod(y|ies) into a sink of %u\n",
              (double)kStemsPerM2, dense, kSinkCapacity);
  std::printf("THE SAME AT 0                yields %u\n", none);

  CHECK(dense < kSinkCapacity,
        "and the sink did not saturate, so the count above is the DENSITY's answer and not the "
        "capacity's -- a number that reads its own cap measures the cap");

  CHECK(dense > 0,
        "**A PLACEMENT GENERATOR YIELDS BODIES**: the whole chain -- a ground query, a snapshot, "
        "a `Ground`, a region pool's sink, a `Yield`, and `Making::Occupy` -- is written in this "
        "tree and was walked by nothing. Unreal's PCG is a plugin with its own registry outside "
        "the engine module, and this suite links no engine source at all, so what stands here is "
        "the generator tier on its own");

  CHECK(none == 0,
        "and the control is the declaration itself: the identical ground asked at ZERO density "
        "yields nothing, so what the count above measures is the stems-per-square-metre the "
        "scenario declares and not the block underneath it");

  Covers("the generator tier: a placement generator reads a ground and yields bodies, reached "
         "without the engine -- board:1948's second door, which had four implementers and no "
         "caller");
  return Report();
}
