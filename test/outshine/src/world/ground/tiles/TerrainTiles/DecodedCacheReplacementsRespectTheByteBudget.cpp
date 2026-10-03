#include "Check.h"
#include "TerrainTiles.h"

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Data::TileId a{.Zoom = 3, .X = 1, .Y = 1};
  const Data::TileId b{.Zoom = 3, .X = 2, .Y = 1};
  const Data::TileId c{.Zoom = 3, .X = 3, .Y = 1};
  TerrainField small(2, 2);
  TerrainField large(4, 4);
  large.SetM(0, 0, 9);
  DecodedCache cache(small.Bytes() + large.Bytes());
  cache.Store(a, small);
  cache.Store(b, small);
  cache.Store(a, large);
  CHECK(cache.Bytes() == small.Bytes() + large.Bytes(),
        "replacement counts the new payload without retaining the old payload");
  TerrainField held;
  CHECK(cache.Take(b, &held), "a larger replacement preserves an entry that still fits");
  CHECK(cache.Take(a, &held) && held.Rows() == 4 && held.Data()[0] == 9,
        "the replacement publishes its new dimensions and heights");
  cache.Store(c, small);
  CHECK(!cache.Take(b, &held), "growth evicts the least recently used other entry");
  CHECK(cache.Take(a, &held) && held.Rows() == 4 && cache.Take(c, &held),
        "the replacement and the newly admitted entry remain available");
  CHECK(cache.Bytes() == small.Bytes() + large.Bytes(), "eviction stays within the byte budget");
  small.SetM(0, 0, 7);
  cache.Store(a, small);
  CHECK(cache.Take(a, &held) && held.Rows() == 2 && held.Data()[0] == 7,
        "a smaller replacement also updates the payload");
  CHECK(cache.Bytes() == 2 * small.Bytes(), "a smaller replacement releases the old accounting");
  return Report();
}
