#include "Check.h"
#include "TerrainTiles.h"
#include <cstddef>
#include <limits>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Data::TileId a{.Zoom = 3, .X = 1, .Y = 1};
  const Data::TileId b{.Zoom = 3, .X = 2, .Y = 1};
  const Data::TileId c{.Zoom = 3, .X = 3, .Y = 1};
  TerrainField source(2, 2);
  source.SetM(0, 0, 17);
  const size_t bytes = source.Bytes();
  DecodedCache cache(3 * bytes);
  cache.Store(a, source);
  cache.Store(b, source);
  cache.Store(c, source);
  auto first = cache.Take(a);
  CHECK(first && cache.Take(c), "pressure starts with complete immutable fields");
  CHECK(cache.EvictUnused(0) == 0 && cache.Bytes() == 3 * bytes,
        "zero pressure preserves every cached input");
  CHECK(cache.EvictUnused(1) == bytes && !cache.Take(b) && cache.Take(c),
        "pressure evicts the least recently used unleased field");
  auto last = cache.Take(c);
  CHECK(first && last, "both remaining fields can be leased");
  if (!first || !last) { return Report(); }
  CHECK(cache.EvictUnused(std::numeric_limits<size_t>::max()) == 0 && cache.Bytes() == 2 * bytes &&
            first->AtM(0, 0) == 17 && last->AtM(0, 0) == 17,
        "even total pressure cannot reclaim a live reader's samples");
  std::weak_ptr<const TerrainField> released = first;
  first.reset();
  CHECK(cache.EvictUnused(1) == bytes && released.expired() && !cache.Take(a),
        "releasing a reader permits actual destruction of its cached payload");
  cache.Store(a, source);
  first = cache.Take(a);
  source.SetM(0, 0, 42);
  cache.Store(a, source);
  CHECK(first && first->AtM(0, 0) == 17,
        "replacement preserves a pre-existing reader's original samples");
  CHECK(cache.EvictUnused(std::numeric_limits<size_t>::max()) == bytes && !cache.Take(a) &&
            cache.Bytes() == bytes && last->AtM(0, 0) == 17,
        "pressure reclaims the unleased replacement and preserves the other pinned field");
  released = last;
  last.reset();
  CHECK(cache.EvictUnused(std::numeric_limits<size_t>::max()) == bytes && released.expired() &&
            cache.Bytes() == 0 && cache.ReadCounters().Evictions == 4,
        "pressure reports every destroyed cached payload without counting old external leases");
  return Report();
}
