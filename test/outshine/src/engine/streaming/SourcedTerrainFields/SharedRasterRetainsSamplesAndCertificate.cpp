#include "Check.h"
#include "SourcedTerrainFields.h"
#include "TerrainRevisionIndex.h"

#include <algorithm>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Data::TileId tile{.Zoom = 2, .X = 1, .Y = 1};
  auto revisions = Ground::TerrainRevisionIndex::Create();
  CHECK(revisions.has_value(), "source owns delivery revisions");
  if (!revisions) { return Report(); }
  const auto stamp = (**revisions).IssueDeliveryStamp(tile);
  CHECK(stamp.has_value(), "raster has a certified delivery");
  if (!stamp) { return Report(); }
  auto raster = std::make_shared<Ground::TerrainField>(3, 3);
  for (uint32_t y = 0; y < 3; ++y) {
    for (uint32_t x = 0; x < 3; ++x) {
      raster->SetM(y, x, static_cast<float>(10u + 2u * x + 4u * y));
    }
  }
  raster->AddSource(
      {.Kind = Data::DataKind::Elevation, .Tile = tile, .SourceId = "dem", .Revision = "r1"});
  raster->SetCertificate(Ground::TerrainCertificate::FromDelivery(tile, *stamp, 7));
  const std::weak_ptr<const Ground::TerrainField> lifetime = raster;
  std::vector<SourcedTerrainFields::Entry> sources{{tile, raster}};
  Ground::HeightField::Block pinned, copied, missing;
  CHECK(SourcedTerrainFields::Share(sources, tile, pinned) && pinned.Terrain == raster &&
            pinned.Nodes.empty(),
        "exact raster pins existing storage without a second sample array");
  CHECK(SourcedTerrainFields::Copy(sources, tile, copied), "independent copy is available");
  CHECK(!SourcedTerrainFields::Share(sources, {.Zoom = 2, .X = 2, .Y = 1}, missing),
        "uncovered region cannot borrow a neighbouring raster");
  const auto shared = Ground::HeightField::Of(2, {std::move(pinned)});
  const auto owned = Ground::HeightField::Of(2, {std::move(copied)});
  sources.clear();
  raster.reset();
  CHECK(!lifetime.expired(), "height snapshot retains storage after provider release");
  CHECK(shared->Qualified() && shared->Certificate().IsComplete() &&
            shared->Certificate().ScopeCurrent(7) && !shared->Certificate().ScopeCurrent(8) &&
            (**revisions).AreCurrent(shared->Certificate().Dependencies()),
        "sharing preserves source qualification and revision boundaries");
  CHECK(shared->RasterDigest() == owned->RasterDigest() &&
            std::ranges::equal(shared->Sources(), owned->Sources()),
        "ownership representation does not change raster identity or provenance");
  const auto point = Ground::TileFracToGeo({.X = 1.5, .Y = 1.5}, 2);
  const auto height =
      shared->At({.LongitudeDeg = point.LongitudeDeg, .LatitudeDeg = point.LatitudeDeg}).AslM();
  CHECK(height && *height == 16.0, "analytic centre height survives ownership transfer");
  return Report();
}
