#include "StructureBake.h"
#include "BuildingMesh.h"
#include "Check.h"
#include <expected>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Generators;

class ChangedMaterial final : public BuildingMesh {
public:
  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    auto built = BuildingMesh::Mesh(plan, scratch, into);
    if (built && !into.WallMaterialM.empty()) { into.WallMaterialM.front() += 1.0f; }
    return built;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double kMinLatDeg = 48.2;
  constexpr double kMinLonDeg = 16.37;
  constexpr double kMaxLatDeg = 48.2002;
  constexpr double kMaxLonDeg = 16.3706;
  constexpr double kBuildingHeightM = 12;
  constexpr double kTileSpanM = 1000;
  RawTile raw;
  raw.LatLon = {kMinLatDeg,
                kMinLonDeg,
                kMinLatDeg,
                kMaxLonDeg,
                kMaxLatDeg,
                kMaxLonDeg,
                kMaxLatDeg,
                kMinLonDeg};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 16.37, .MinLatDeg = 48.2, .MaxLonDeg = 16.378, .MaxLatDeg = 48.208};
  const auto cell = StructureCellOf(bounds, raw.LatLon);
  CHECK(cell.has_value(), "the native digest fixture has a valid cell");
  if (!cell) { return Report(); }
  raw.Structures.push_back({.PointCount = 4,
                            .SourceFirst = 12,
                            .Cell = *cell,
                            .HeightM = kBuildingHeightM,
                            .Pitched = 1,
                            .WallColour = std::nullopt,
                            .Facade = FacadeStyle::House});
  raw.RequestedDetail = LevelOfDetail::Shell;
  raw.RequestedCell = cell->Index;
  raw.TileSpanM = kTileSpanM;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 100.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "the digest fixture has real height input");
  if (!heights) { return Report(); }
  const BuildingMesh original;
  const ChangedMaterial changed;
  auto scratch = original.Scratch();
  BakedTile first;
  BakedTile second;
  CHECK(BakeStructures(raw, *heights, original, *scratch, first), "the real producer bakes");
  CHECK(BakeStructures(raw, *heights, changed, *scratch, second), "the material counter bakes");
  CHECK(!first.Built.WallMaterialM.empty(), "the native recipe emits the optional metric data");
  CHECK(first.Built.WallCorners.size() == second.Built.WallCorners.size() &&
            first.Built.WallRun == second.Built.WallRun &&
            first.Built.RoofRun == second.Built.RoofRun,
        "the counter preserves native geometry and connectivity");
  CHECK(first.Built.WallMaterialM != second.Built.WallMaterialM && first.Digest != second.Digest,
        "changing only material metres changes the actual baked product digest");
  return Report();
}
