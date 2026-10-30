#include "Check.h"
#include "TerrainFieldSet.h"
#include "TerrainGrid.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr Data::TileId kWest{.Zoom = 1, .X = 0, .Y = 0};
constexpr Data::TileId kEast{.Zoom = 1, .X = 1, .Y = 0};

std::shared_ptr<Ground::TerrainField> Field(Data::TileId tile, float height) {
  auto field = std::make_shared<Ground::TerrainField>(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t col = 0; col < 2; ++col) { field->SetM(row, col, height); }
  }
  field->AddSource(
      {.Kind = Data::DataKind::Elevation, .Tile = tile, .SourceId = "bound-dem", .Revision = "r1"});
  return field;
}

void CheckBindings() {
  const auto west = Field(kWest, 0);
  const auto east = Field(kEast, 20);
  const TerrainFieldSet first({{kWest, west}, {kEast, east}});
  const TerrainFieldSet reverse({{kEast, east}, {kWest, west}});
  const auto legacy = first.BindingAt(1);
  const auto native = first.BilinearBindingAt(1, "source datum/1", "source-as-model/1");
  const auto same = reverse.BilinearBindingAt(1, "source datum/1", "source-as-model/1");
  CHECK(native && same && legacy && native->Key == same->Key && native->Key != legacy->Key,
        "field order stays irrelevant while the explicit bilinear sampler cannot collide with "
        "legacy sampling");
  if (!native) { return; }
  const auto datum = first.BilinearBindingAt(1, "source datum/2", "source-as-model/1");
  const auto mapping = first.BilinearBindingAt(1, "source datum/1", "source-as-model/2");
  CHECK(datum && mapping && datum->Key != native->Key && mapping->Key != native->Key &&
            datum->Key != mapping->Key,
        "datum and model-height interpretation are separate persistent inputs");
  const TerrainFieldSet changed({{kWest, Field(kWest, 1)}, {kEast, east}});
  const auto raster = changed.BilinearBindingAt(1, "source datum/1", "source-as-model/1");
  CHECK(raster && raster->Key != native->Key,
        "changed original raster values change the same existing input identity");
  CHECK(native->Fields == 2 && native->RasterBytes == 8 * sizeof(float) &&
            native->CopiedRasterBytes ==
                (std::endian::native == std::endian::little ? 0 : 8 * sizeof(float)),
        "bilinear binding reports original raster reads and avoids raster copies on the target "
        "little-endian platform");
  CHECK(!first.BilinearBindingAt(1, "", "source-as-model/1") &&
            !first.BilinearBindingAt(1, "source datum/1", "") &&
            !first.BilinearBindingAt(-1, "source datum/1", "source-as-model/1"),
        "an absent datum, mapping or valid geographic zoom cannot produce a bound input");
  Ground::HeightField::Block block;
  CHECK(first.ShareOriginalField(kWest, block) && block.Terrain == west && block.Nodes.empty(),
        "the exact original raster reaches its existing snapshot lease without resampling");
  const Data::TileId child{.Zoom = 2, .X = 0, .Y = 0};
  CHECK(
      !first.ShareOriginalField(child, block) &&
          !first.BilinearBindingAt(2, "source datum/1", "source-as-model/1"),
      "missing requested original tiles cannot acquire an unversioned resampled ancestor identity");
  const TerrainFieldSet ambiguous({{kWest, west}, {kWest, Field(kWest, 20)}});
  CHECK(
      !ambiguous.BilinearBindingAt(1, "source datum/1", "source-as-model/1"),
      "ambiguous duplicate source tiles cannot use input order as an implicit selection procedure");
}
}

int main() {
  CheckBindings();
  return Report();
}
