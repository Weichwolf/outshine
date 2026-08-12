/* WHERE ONE TILE'S PRODUCE LIES in a producer's own array, by OsmField tile index.
 *
 * A producer consumes one tile per pass and appends what that tile yielded, so the range exists by
 * construction; keeping it turns "everything decoded so far, filtered" into a slice. Tiles are
 * consumed out of order when one of them defers (world/TileWatermark.h), which is why this is
 * indexed by tile and not a running list, and why a tile that yielded nothing still has an entry. */
#ifndef TILERANGES_H
#define TILERANGES_H

#include <cstdint>
#include <vector>

#include "Capacity.h"

namespace outshine::World {

class TileRanges {
public:
  struct Range {
    uint32_t First = 0, Count = 0;
  };

  void Set(uint32_t tile, uint32_t first, uint32_t end) {
    if ((size_t)tile >= Ranges_.size()) Ranges_.resize((size_t)tile + 1);
    Ranges_[tile] = Range{first, end - first};
  }
  Range At(uint32_t tile) const {
    return (size_t)tile < Ranges_.size() ? Ranges_[tile] : Range{};
  }
  size_t HeapBytes() const { return CapacityBytes(Ranges_); }

private:
  std::vector<Range> Ranges_;
};

} // namespace outshine::World
#endif
