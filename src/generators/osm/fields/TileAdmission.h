#ifndef OUTSHINE_GENERATORS_OSM_FIELDS_TILEADMISSION_H
#define OUTSHINE_GENERATORS_OSM_FIELDS_TILEADMISSION_H

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <span>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"

namespace outshine::Generators::Osm {

using namespace outshine::Ground;

constexpr int kEveryRing = 1 << 20;
constexpr uint64_t kNoAdmissionKey = 0xffffffffffffffffull;
constexpr unsigned kAwayShift = 48u;
constexpr unsigned kZoomShift = 40u;
constexpr unsigned kSidewaysShift = 20u;

class TileAdmission {
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
  Next Ask(std::span<const OsmField::Tile> tiles, Reach over, Consumable consumable) {
    const int centreX = over.CentreX;
    const int centreY = over.CentreY;
    Candidates_.clear();
    for (size_t index = 0; index < tiles.size(); ++index) {
      const auto tile = static_cast<uint32_t>(index);
      const auto &source = tiles[index];
      if (source.FeatureCount == 0 || Taken(tile)) { continue; }
      if (Beyond(tiles, tile, over)) {
        Skipped_.insert(std::ranges::lower_bound(Skipped_, tile), tile);
        Unavailable_.insert(std::ranges::lower_bound(Unavailable_, tile), tile);
        ++Beyond_;
      } else {
        Candidates_.push_back({.From = source.FirstFeature,
                               .To = static_cast<size_t>(source.FirstFeature) + source.FeatureCount,
                               .Tile = tile,
                               .Found = true});
      }
    }
    const auto key = [tiles, centreX, centreY](const Next &one) {
      if (one.Tile >= tiles.size()) { return kNoAdmissionKey; }
      const OsmField::Tile &which = tiles[one.Tile];
      const int64_t across = static_cast<int64_t>(which.X) - centreX;
      const int64_t down = static_cast<int64_t>(which.Y) - centreY;
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
    Unavailable_.insert(std::ranges::lower_bound(Unavailable_, tile), tile);
  }

  void Release(uint32_t tile) {
    const auto at = std::ranges::lower_bound(Unavailable_, tile);
    assert(at != Unavailable_.end() && *at == tile && Takes_ > 0);
    Unavailable_.erase(at);
    --Takes_;
  }

  size_t ReleaseUnaccepted(std::span<const uint32_t> accepted) {
    assert(accepted.size() <= Takes_);
    const size_t released = Takes_ - accepted.size();
    if (released == 0) { return 0; }
    Unavailable_.clear();
    Unavailable_.reserve(accepted.size() + Skipped_.size());
    std::ranges::set_union(accepted, Skipped_, std::back_inserter(Unavailable_));
    Takes_ = accepted.size();
    return released;
  }

  [[nodiscard]] bool Done(std::span<const OsmField::Tile> tiles) const {
    for (size_t tile = 0; tile < tiles.size(); ++tile) {
      if (tiles[tile].FeatureCount != 0 && !Taken(static_cast<uint32_t>(tile))) { return false; }
    }
    return true;
  }

  [[nodiscard]] bool
  AcceptedWithin(std::span<const OsmField::Tile> tiles, int centreX, int centreY, int rings) const {
    if (rings < 0) { return false; }
    for (size_t tile = 0; tile < tiles.size(); ++tile) {
      const auto &source = tiles[tile];
      if (source.FeatureCount == 0 || std::abs(source.X - centreX) > rings ||
          std::abs(source.Y - centreY) > rings) {
        continue;
      }
      const auto index = static_cast<uint32_t>(tile);
      if (std::ranges::binary_search(Skipped_, index) || !Taken(index)) { return false; }
    }
    return true;
  }

  [[nodiscard]] int Deferrals() const { return Deferrals_; }

  [[nodiscard]] size_t UnavailableCount() const { return Unavailable_.size(); }

  [[nodiscard]] size_t BeyondCount() const { return Beyond_; }

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Unavailable_) + CapacityBytes(Skipped_) + CapacityBytes(Candidates_);
  }

private:
  [[nodiscard]] bool Taken(uint32_t tile) const {
    return std::ranges::binary_search(Unavailable_, tile);
  }

  [[nodiscard]] static bool
  Beyond(std::span<const OsmField::Tile> tiles, uint32_t tile, Reach over) {
    if (over.Rings >= kEveryRing || tile >= tiles.size()) { return false; }
    const OsmField::Tile &which = tiles[tile];
    const int across = which.X - over.CentreX;
    const int down = which.Y - over.CentreY;
    return std::abs(across) > over.Rings || std::abs(down) > over.Rings;
  }

  std::vector<uint32_t> Unavailable_;
  std::vector<uint32_t> Skipped_;
  size_t Beyond_ = 0;
  std::vector<Next> Candidates_;
  size_t Takes_ = 0;
  int Deferrals_ = 0;
};

}
#endif
