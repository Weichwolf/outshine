#ifndef OUTSHINE_WORLD_GROUND_TILEWATERMARK_H
#define OUTSHINE_WORLD_GROUND_TILEWATERMARK_H

#include <algorithm>
#include <cassert>
#include <span>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"

namespace outshine::Ground {

constexpr int kEveryRing = 1 << 20;
constexpr uint64_t kNoWatermark = 0xffffffffffffffffull;
constexpr unsigned kAwayShift = 48u;
constexpr unsigned kZoomShift = 40u;
constexpr unsigned kSidewaysShift = 20u;

class TileWatermark {
public:
  struct Next {
    size_t From = 0, To = 0;
    uint32_t Tile = 0;
    bool Found = false;
  };

  struct Reach {
    int CentreX = 0;
    int CentreY = 0;
    int Rings = kEveryRing;
    size_t CandidatesMost = 0;
  };

  template <typename Consumable>
  Next Ask(std::span<const OsmField::Feature> feats,
           std::span<const OsmField::Tile> tiles,
           Reach over,
           Consumable consumable) {
    const int centreX = over.CentreX;
    const int centreY = over.CentreY;
    Candidates_.clear();
    size_t at = Mark_;
    while (at < feats.size()) {
      const uint32_t tile = feats[at].Tile;
      size_t end = at;
      while (end < feats.size() && feats[end].Tile == tile) { end++; }
      if (!Taken(tile)) {
        if (Beyond(tiles, tile, over)) {
          Skipped_.insert(std::ranges::lower_bound(Skipped_, tile), tile);
          Ahead_.insert(std::ranges::lower_bound(Ahead_, tile), tile);
          ++Beyond_;
        } else {
          Candidates_.push_back(Next{.From = at, .To = end, .Tile = tile, .Found = true});
        }
      }
      at = end;
    }
    const auto key = [tiles, centreX, centreY](const Next &one) {
      if (one.Tile >= tiles.size()) { return kNoWatermark; }
      const OsmField::Tile &which = tiles[one.Tile];
      const long across = static_cast<long>(which.X) - static_cast<long>(centreX);
      const long down = static_cast<long>(which.Y) - static_cast<long>(centreY);
      const unsigned long long away =
          static_cast<unsigned long long>(across * across + down * down) & 0xffffull;
      const unsigned long long zoom = static_cast<unsigned long long>(which.Z) & 0xffull;
      const unsigned long long sideways = static_cast<unsigned long long>(which.X) & 0xfffffull;
      const unsigned long long along = static_cast<unsigned long long>(which.Y) & 0xfffffull;
      return (away << kAwayShift) | (zoom << kZoomShift) | (sideways << kSidewaysShift) | along;
    };
    std::sort(Candidates_.begin(), Candidates_.end(), [&key](const Next &a, const Next &b) {
      return key(a) < key(b);
    });
    size_t attempted = 0;
    for (const Next &one : Candidates_) {
      if (over.CandidatesMost > 0 && attempted >= over.CandidatesMost) { break; }
      ++attempted;
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

  void Release(uint32_t tile) {
    const auto at = std::ranges::lower_bound(Ahead_, tile);
    assert(at != Ahead_.end() && *at == tile && Takes_ > 0);
    Ahead_.erase(at);
    --Takes_;
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

  [[nodiscard]] bool AcceptedWithin(std::span<const OsmField::Feature> feats,
                                    std::span<const OsmField::Tile> tiles,
                                    int centreX,
                                    int centreY,
                                    int rings) const {
    if (rings < 0) { return false; }
    size_t at = 0;
    while (at < feats.size()) {
      const size_t first = at;
      const uint32_t tile = feats[at].Tile;
      while (at < feats.size() && feats[at].Tile == tile) { ++at; }
      if (tile >= tiles.size()) { return false; }
      const OsmField::Tile &source = tiles[tile];
      if (std::abs(source.X - centreX) > rings || std::abs(source.Y - centreY) > rings) {
        continue;
      }
      const bool skipped = std::ranges::binary_search(Skipped_, tile);
      if (skipped || (first >= Mark_ && !Taken(tile))) { return false; }
    }
    return true;
  }

  [[nodiscard]] int Deferrals() const { return Deferrals_; }

  [[nodiscard]] size_t AheadCount() const { return Ahead_.size(); }

  [[nodiscard]] size_t BeyondCount() const { return Beyond_; }

  [[nodiscard]] size_t HeapBytes() const { return CapacityBytes(Ahead_) + CapacityBytes(Skipped_); }

private:
  [[nodiscard]] bool Taken(uint32_t tile) const { return std::ranges::binary_search(Ahead_, tile); }

  [[nodiscard]] static bool
  Beyond(std::span<const OsmField::Tile> tiles, uint32_t tile, Reach over) {
    if (over.Rings >= kEveryRing || tile >= tiles.size()) { return false; }
    const OsmField::Tile &which = tiles[tile];
    const int across = which.X - over.CentreX;
    const int down = which.Y - over.CentreY;
    return std::abs(across) > over.Rings || std::abs(down) > over.Rings;
  }

  std::vector<uint32_t> Ahead_;
  std::vector<uint32_t> Skipped_;
  size_t Beyond_ = 0;
  size_t Mark_ = 0;
  std::vector<Next> Candidates_;
  size_t Takes_ = 0;
  int Deferrals_ = 0;
};

}
#endif
