/* HOW FAR A CONSUMER OF THE VECTOR FIELD HAS GOT, tile by tile.
 *
 * The features of one tile are contiguous, so a consumer that works a tile at a time can keep one
 * index — until a tile is not consumable yet. One watermark alone then made that tile stall every
 * LATER tile's features, so one hole in the elevation emptied the whole street behind it, and a ring
 * that travels turns that into a hole that grows.
 *
 * A deferred tile is therefore STEPPED OVER and asked again, and what was taken ahead of the mark is
 * kept until the mark reaches it. The set is bounded by how many deferred tiles stand between the
 * mark and the newest arrival, so the scan stays short. */
#ifndef TILEWATERMARK_H
#define TILEWATERMARK_H

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"

namespace outshine::World {

class TileWatermark {
public:
  /* The next tile this consumer could take, and where its features end. None when there is nothing
   * left that has not been taken. */
  struct Next {
    size_t From = 0, To = 0;
    uint32_t Tile = 0;
    bool Found = false;
  };

  /* Walks from the mark and hands back the first tile that is neither taken nor refused by
   * `consumable`. Every tile it steps over that was not taken counts as a deferral. */
  template <typename Consumable>
  Next Ask(const std::vector<OsmField::Feature> &feats, Consumable consumable) {
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

  /* Follows the contiguous prefix that has been taken and empties the out-of-order set behind it:
   * what is left in it is exactly the tiles still deferred. */
  void Advance(const std::vector<OsmField::Feature> &feats) {
    while (Mark_ < feats.size() && Taken(feats[Mark_].Tile)) {
      const uint32_t tile = feats[Mark_].Tile;
      while (Mark_ < feats.size() && feats[Mark_].Tile == tile) Mark_++;
      Ahead_.erase(std::lower_bound(Ahead_.begin(), Ahead_.end(), tile));
    }
  }

  bool Done(const std::vector<OsmField::Feature> &feats) const { return Mark_ >= feats.size(); }
  int Deferrals() const { return Deferrals_; }
  size_t AheadCount() const { return Ahead_.size(); }
  size_t HeapBytes() const { return CapacityBytes(Ahead_); }

private:
  bool Taken(uint32_t tile) const {
    return std::binary_search(Ahead_.begin(), Ahead_.end(), tile);
  }

  std::vector<uint32_t> Ahead_;
  size_t Mark_ = 0;
  int Deferrals_ = 0;
};

} // namespace outshine::World
#endif
