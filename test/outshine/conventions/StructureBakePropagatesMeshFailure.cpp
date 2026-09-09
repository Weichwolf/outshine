#include "../../../src/generators/building/StructureBake.h"
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
  return Report();
}
