#include <atomic>

#include "src/generators/building/StructureBake.h"
#include "Check.h"

namespace {
struct Scratch final : outshine::MeshScratch {};

class RefusingMesher final : public outshine::StructureMesher {
public:
  explicit RefusingMesher(outshine::StructureMeshError error) : Error_(error) {}

  std::unique_ptr<outshine::MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>();
  }

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &,
       outshine::MeshScratch &,
       outshine::Raised &) const noexcept override {
    ++Calls;
    return std::unexpected(Error_);
  }

  mutable size_t Calls = 0;

private:
  outshine::StructureMeshError Error_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::RawTile raw;
  raw.LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  raw.Structures.push_back({.PointCount = 4, .HeightM = 6});
  raw.FocalPx = 1000;
  raw.AwayM = 1;
  raw.TileSpanM = 1000;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  const auto heights = Ground::HeightField::Of(0, {block});
  for (auto error : {StructureMeshError::InvalidPlan,
                     StructureMeshError::IncompatibleScratch,
                     StructureMeshError::BuildFailed}) {
    RefusingMesher mesher(error);
    auto scratch = mesher.Scratch();
    Generators::BakedTile output;
    const auto result = Generators::BakeStructures(raw, *heights, mesher, *scratch, output);
    CHECK(mesher.Calls == 1, "fixture reaches actual per-building mesher");
    CHECK(!result, "mesh rejection cannot become a successful tile bake");
    if (!result) {
      const auto *reason = std::get_if<StructureMeshError>(&result.error());
      CHECK(reason && *reason == error, "specific mesher error reaches bake boundary");
      CHECK(!Generators::Describe(result.error()).empty(),
            "diagnostic is available without allocation");
    }
  }
  raw.Structures.push_back(raw.Structures.front());
  RefusingMesher unsupported(StructureMeshError::UnsupportedFootprint);
  auto scratch = unsupported.Scratch();
  Generators::BakedTile output;
  const auto result = Generators::BakeStructures(raw, *heights, unsupported, *scratch, output);
  CHECK(result && unsupported.Calls == 2 && output.UnsupportedMeshes == 2,
        "unsupported forms are counted and processing continues to the next building");
  raw.LatLon.insert(raw.LatLon.end(), {47, 8.9998, 47.0001, 8.9998});
  raw.Ways.push_back({.LocalFirst = 4, .PointCount = 2, .HalfWidthM = 4});
  RefusingMesher frontedMesher(StructureMeshError::UnsupportedFootprint);
  auto frontedScratch = frontedMesher.Scratch();
  Generators::BakedTile fronted;
  const auto frontedResult =
      Generators::BakeStructures(raw, *heights, frontedMesher, *frontedScratch, fronted);
  CHECK(frontedResult && fronted.Fronted == 2,
        "a nearby road line gives every building in its tile a known frontage");

  RefusingMesher oneShotMesher(StructureMeshError::UnsupportedFootprint);
  auto oneShotScratch = oneShotMesher.Scratch();
  Generators::BakedTile oneShot;
  const auto oneShotResult =
      Generators::BakeStructures(raw, *heights, oneShotMesher, *oneShotScratch, oneShot);
  RefusingMesher slicedMesher(StructureMeshError::UnsupportedFootprint);
  auto slicedScratch = slicedMesher.Scratch();
  Generators::StructureBakeProgress progress;
  Generators::BakedTile sliced;
  const auto first = progress.Advance(raw, *heights, slicedMesher, *slicedScratch, sliced, 1);
  const auto second = progress.Advance(raw, *heights, slicedMesher, *slicedScratch, sliced, 1);
  CHECK(oneShotResult && first && second && !*first && *second,
        "a bounded bake retains its aggregate until its final structure range");
  CHECK(slicedMesher.Calls == oneShotMesher.Calls &&
            sliced.Prints.size() == oneShot.Prints.size() &&
            sliced.UnsupportedMeshes == oneShot.UnsupportedMeshes &&
            sliced.Built.WallRun == oneShot.Built.WallRun &&
            sliced.Built.RoofRun == oneShot.Built.RoofRun,
        "one-structure ranges produce the same complete tile as one uninterrupted bake");

  RefusingMesher cancelledMesher(StructureMeshError::UnsupportedFootprint);
  auto cancelledScratch = cancelledMesher.Scratch();
  Generators::StructureBakeProgress cancelled;
  Generators::BakedTile cancelledOutput;
  std::atomic_bool stopping{false};
  const auto beforeCancel = cancelled.Advance(
      raw, *heights, cancelledMesher, *cancelledScratch, cancelledOutput, 1, &stopping);
  stopping.store(true);
  const auto afterCancel = cancelled.Advance(
      raw, *heights, cancelledMesher, *cancelledScratch, cancelledOutput, 1, &stopping);
  CHECK(beforeCancel && !*beforeCancel && !afterCancel,
        "cancelling between ranges cannot report a complete tile");
  if (!afterCancel) {
    const auto *reason = std::get_if<Generators::StructureBakeErrorKind>(&afterCancel.error());
    CHECK(reason && *reason == Generators::StructureBakeErrorKind::Cancelled,
          "cancellation reaches the generator boundary as its explicit error");
  }
  return Report();
}
