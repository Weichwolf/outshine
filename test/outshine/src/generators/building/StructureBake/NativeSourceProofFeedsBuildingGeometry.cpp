#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "StructureInput.h"

#include <memory>
#include <utility>

namespace {
class NativeObjects final : public outshine::Data::SourceObjects {
public:
  [[nodiscard]] bool Contains(outshine::Data::SourceObjectId id) const noexcept override {
    return id.Id == 42 && id.Kind == 201;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  outshine::Ground::StructureFootprints footprints;
  footprints.Origin.Bounds = {.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1};
  footprints.LatLon = {0, 0, 0, 0.001, 0.001, 0.001, 0.001, 0};
  footprints.Structures.push_back(
      {.First = 0,
       .Count = 4,
       .Height = {.TopM = 13,
                  .MinimumM = 3,
                  .TopOrigin = outshine::Ground::BuildingHeightOrigin::Declared,
                  .MinimumOrigin = outshine::Ground::BuildingHeightOrigin::Declared,
                  .ConflictingLevels = false},
       .Source = {.Id = 42, .Kind = 201}});
  auto input = StructureInput(std::move(footprints));
  CHECK(input.has_value(), "a native producer supplies a footprint and its opaque source ID");
  if (!input) { return Report(); }
  auto source = std::make_shared<const NativeObjects>();
  const std::weak_ptr<const NativeObjects> held = source;
  input->SourceInputs.Objects = source;
  source.reset();
  CHECK(!held.expired(), "the worker owns the consumed input proof after producer release");
  input->TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 0, .LatitudeDeg = 0, .HeightM = 100}, input->AnchorEcef);
  outshine::Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 100);
  const auto heights = outshine::Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "independent flat terrain is available");
  if (!heights) { return Report(); }
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile product;
  CHECK(BakeStructures(*input, *heights, mesher, *scratch, product) && product.Prints.size() == 1 &&
            product.Prints.front().HeightM == 13 && product.Prints.front().MinimumHeightM == 3 &&
            !product.Built.WallCorners.empty(),
        "actual building geometry accepts a non-OSM object kind and preserves the height interval");
  input->Structures.front().SourceId.Id = 43;
  BakedTile missing;
  CHECK(!BakeStructures(*input, *heights, mesher, *scratch, missing),
        "an ID absent from the consumed proof cannot generate a product");
  input->Structures.front().SourceId = {.Id = 42, .Kind = 200};
  BakedTile wrongKind;
  CHECK(!BakeStructures(*input, *heights, mesher, *scratch, wrongKind),
        "equal numeric IDs of distinct source kinds remain distinct");
  input->SourceInputs = {};
  CHECK(held.expired() && !product.Built.WallCorners.empty(),
        "the generated geometry remains usable after its consumed proof is retired");
  return Report();
}
