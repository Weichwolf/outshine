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
    if (static_cast<size_t>(tile) >= Ranges_.size()) {
      Ranges_.resize(static_cast<size_t>(tile) + 1);
    }
    Ranges_[tile] = Range{.First = first, .Count = end - first};
  }

  void Prepare(uint32_t tile) { Ranges_.reserve(static_cast<size_t>(tile) + 1u); }

  struct Shift {
    uint32_t AfterTile = 0;
    uint32_t By = 0;
  };

  void ShiftAfter(Shift change) noexcept {
    for (size_t at = static_cast<size_t>(change.AfterTile) + 1u; at < Ranges_.size(); ++at) {
      if (Ranges_[at].Count > 0) { Ranges_[at].First += change.By; }
    }
  }

  [[nodiscard]] Range At(uint32_t tile) const {
    return static_cast<size_t>(tile) < Ranges_.size() ? Ranges_[tile] : Range{};
  }

  [[nodiscard]] size_t HeapBytes() const { return CapacityBytes(Ranges_); }

private:
  std::vector<Range> Ranges_;
};

}
#endif
