#include "Check.h"
#include "HeightField.h"
#include <algorithm>
#include <array>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Data::TileSourceIdentity west{.Kind = Data::DataKind::Elevation,
                                      .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                      .SourceId = "dem",
                                      .Revision = "west-1"};
  const Data::TileSourceIdentity east{.Kind = Data::DataKind::Elevation,
                                      .Tile = {.Zoom = 14, .X = 5, .Y = 7},
                                      .SourceId = "dem",
                                      .Revision = "east-1"};
  Ground::HeightField::Block first;
  first.Sources = {east, west};
  first.Nodes = {1, 1, 1, 1};
  first.Raster = {.Side = 2, .Postings = 2};
  Ground::HeightField::Block second = first;
  second.Sources = {east};
  const auto fine = Ground::HeightField::Of(14, {first, second});
  const std::array<Data::TileSourceIdentity, 2> expected{west, east};
  CHECK(fine->Qualified() && std::ranges::equal(fine->Sources(), expected),
        "fine input owns the sorted unique source union of all blocks");
  second.Sources.front().Revision = "east-2";
  const auto changed = Ground::HeightField::Of(14, {first, second});
  CHECK(changed->Qualified() && !std::ranges::equal(changed->Sources(), fine->Sources()),
        "equal height samples from a different source revision are different input");
  first.Nodes[0] = 2;
  const auto changedRaster = Ground::HeightField::Of(14, {first, second});
  CHECK(changedRaster->RasterDigest() != changed->RasterDigest() &&
            std::ranges::equal(changedRaster->Sources(), changed->Sources()),
        "changed raster samples invalidate a bake even when source identities are unchanged");
  second.Sources.clear();
  const auto missing = Ground::HeightField::Of(14, {first, second});
  CHECK(!missing->Qualified() && !missing->Sources().empty(),
        "one unidentified fine block disqualifies the entire bake input");
  const auto sampled = Ground::HeightField::Of(14, {first}, true);
  CHECK(!sampled->Qualified() && !sampled->Sources().empty(),
        "scalar fallback remains unqualified even beside identified blocks");
  return Report();
}
