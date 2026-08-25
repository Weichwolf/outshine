#ifndef OUTSHINE_WORLD_GROUND_TILERANGES_H
#define OUTSHINE_WORLD_GROUND_TILERANGES_H

#include <cstdint>
#include <vector>

#include "Capacity.h"

namespace outshine::Ground {

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

}
#endif
