#ifndef TILEWATERMARK_H
#define TILEWATERMARK_H

#include <span>
#include <algorithm>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"

namespace outshine::World {

class TileWatermark {
public:

  struct Next {
    size_t From = 0, To = 0;
    uint32_t Tile = 0;
    bool Found = false;
  };

  template <typename Consumable>
  Next Ask(std::span<const OsmField::Feature> feats, Consumable consumable) {
    Next next;
    size_t at = Mark_;
    while (at < feats.size()) {
      const uint32_t tile = feats[at].Tile;
      size_t end = at;
      while (end < feats.size() && feats[end].Tile == tile) end++;
      if (!Taken(tile)) {
        if (consumable(at, end)) return Next{at, end, tile, true};
        Deferrals_++;
      }
      at = end;
    }
    return next;
  }

  void Take(uint32_t tile) { Ahead_.insert(std::lower_bound(Ahead_.begin(), Ahead_.end(), tile), tile); }

  void Advance(std::span<const OsmField::Feature> feats) {
    while (Mark_ < feats.size() && Taken(feats[Mark_].Tile)) {
      const uint32_t tile = feats[Mark_].Tile;
      while (Mark_ < feats.size() && feats[Mark_].Tile == tile) Mark_++;
      Ahead_.erase(std::lower_bound(Ahead_.begin(), Ahead_.end(), tile));
    }
  }

  [[nodiscard]] bool Done(std::span<const OsmField::Feature> feats) const { return Mark_ >= feats.size(); }
  int Deferrals() const { return Deferrals_; }
  size_t AheadCount() const { return Ahead_.size(); }
  size_t HeapBytes() const { return CapacityBytes(Ahead_); }

private:
  [[nodiscard]] bool Taken(uint32_t tile) const {
    return std::binary_search(Ahead_.begin(), Ahead_.end(), tile);
  }

  std::vector<uint32_t> Ahead_;
  size_t Mark_ = 0;
  int Deferrals_ = 0;
};

}
#endif
