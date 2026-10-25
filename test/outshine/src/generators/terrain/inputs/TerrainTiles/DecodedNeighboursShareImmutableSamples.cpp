#include "Check.h"
#include "TerrainTiles.h"

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Data::TileId at{.Zoom = 3, .X = 1, .Y = 1};
  TerrainField source(512, 512);
  source.SetM(0, 0, 17);
  DecodedCache cache(source.Bytes());
  cache.Store(at, source);
  auto first = cache.Take(at);
  const auto second = cache.Take(at);
  CHECK(first && second, "both readers reach the cached tile");
  if (!first || !second) { return Report(); }
  CHECK(first->Data() == second->Data(), "read-only neighbours share the same sample allocation");
  const auto *samples = first->Data();
  TerrainGrid grid = TerrainGrid::Holding(std::move(first));
  CHECK(grid.TryField()->Data() == samples, "a read-only grid retains the shared samples");
  TerrainField *target = grid.TryFieldMutable();
  CHECK(target != nullptr, "the stitching target obtains writable samples");
  if (target != nullptr) {
    CHECK(target->Data() != samples, "only the stitching target receives a private sample copy");
    target->SetM(0, 0, 23);
    CHECK(grid.TryFieldMutable()->Data() == target->Data(),
          "further writes retain that private copy");
  }
  CHECK(second->AtM(0, 0) == 17, "stitching never changes another reader's cached sample");
  const auto again = cache.Take(at);
  CHECK(again && again->AtM(0, 0) == 17, "stitching never changes the reusable source raster");
  CHECK(cache.Bytes() == source.Bytes(), "shared readers do not change cache accounting");
  return Report();
}
