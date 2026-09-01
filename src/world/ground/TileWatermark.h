#ifndef OUTSHINE_WORLD_GROUND_TILEWATERMARK_H
#define OUTSHINE_WORLD_GROUND_TILEWATERMARK_H

#include <algorithm>
#include <span>
#include <algorithm>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"

namespace outshine::Ground {

class TileWatermark {
public:
  struct Next {
    size_t From = 0, To = 0;
    uint32_t Tile = 0;
    bool Found = false;
  };

  template <typename Consumable>
  Next Ask(std::span<const OsmField::Feature> feats,
           std::span<const OsmField::Tile> tiles,
           int centreX,
           int centreY,
           Consumable consumable) {
    Candidates_.clear();
    size_t at = Mark_;
    while (at < feats.size()) {
      const uint32_t tile = feats[at].Tile;
      size_t end = at;
      while (end < feats.size() && feats[end].Tile == tile) { end++; }
      if (!Taken(tile)) {
        Candidates_.push_back(Next{.From = at, .To = end, .Tile = tile, .Found = true});
      }
      at = end;
    }
    const auto key = [tiles, centreX, centreY](const Next &one) {
      if (one.Tile >= tiles.size()) { return 0xffffffffffffffffull; }
      const OsmField::Tile &which = tiles[one.Tile];
      const long across = static_cast<long>(which.X) - static_cast<long>(centreX);
      const long down = static_cast<long>(which.Y) - static_cast<long>(centreY);
      const unsigned long long away =
          static_cast<unsigned long long>(across * across + down * down) & 0xffffull;
      const unsigned long long zoom = static_cast<unsigned long long>(which.Z) & 0xffull;
      const unsigned long long sideways = static_cast<unsigned long long>(which.X) & 0xfffffull;
      const unsigned long long along = static_cast<unsigned long long>(which.Y) & 0xfffffull;
      return (away << 48u) | (zoom << 40u) | (sideways << 20u) | along;
    };
    std::sort(Candidates_.begin(), Candidates_.end(), [&key](const Next &a, const Next &b) {
      return key(a) < key(b);
    });
    for (const Next &one : Candidates_) {
      if (consumable(one.From, one.To)) { return one; }
      Deferrals_++;
    }
    return Next{};
  }

  [[nodiscard]] size_t Takes() const { return Takes_; }

  void Take(uint32_t tile) {
    Takes_++;
    Ahead_.insert(std::ranges::lower_bound(Ahead_, tile), tile);
  }

  void Advance(std::span<const OsmField::Feature> feats) {
    while (Mark_ < feats.size() && Taken(feats[Mark_].Tile)) {
      const uint32_t tile = feats[Mark_].Tile;
      while (Mark_ < feats.size() && feats[Mark_].Tile == tile) { Mark_++; }
      Ahead_.erase(std::ranges::lower_bound(Ahead_, tile));
    }
  }

  [[nodiscard]] bool Done(std::span<const OsmField::Feature> feats) const {
    return Mark_ >= feats.size();
  }

  [[nodiscard]] int Deferrals() const { return Deferrals_; }

  [[nodiscard]] size_t AheadCount() const { return Ahead_.size(); }

  [[nodiscard]] size_t HeapBytes() const { return CapacityBytes(Ahead_); }

private:
  [[nodiscard]] bool Taken(uint32_t tile) const { return std::ranges::binary_search(Ahead_, tile); }

  std::vector<uint32_t> Ahead_;
  size_t Mark_ = 0;
  std::vector<Next> Candidates_;
  size_t Takes_ = 0;
  int Deferrals_ = 0;
};

} // namespace outshine::Ground
#endif
