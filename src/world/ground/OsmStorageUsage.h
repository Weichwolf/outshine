#ifndef OUTSHINE_WORLD_GROUND_OSMSTORAGEUSAGE_H
#define OUTSHINE_WORLD_GROUND_OSMSTORAGEUSAGE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace outshine::Ground {

struct OsmStorageUsage {
  size_t Features = 0;
  size_t Rings = 0;
  size_t Points = 0;
  size_t Tags = 0;
  size_t Values = 0;
  size_t Keys = 0;
  size_t Strings = 0;
  size_t Tiles = 0;

  [[nodiscard]] constexpr bool TryAdd(const OsmStorageUsage &growth) noexcept {
    constexpr size_t indices = std::numeric_limits<uint32_t>::max();
    constexpr size_t signedIndices = std::numeric_limits<int>::max();
    constexpr size_t pointPairs = std::min(indices, std::numeric_limits<size_t>::max() / 2);
    if (!Fits(Features, growth.Features, signedIndices) ||
        !Fits(Tiles, growth.Tiles, signedIndices) || !Fits(Rings, growth.Rings, indices) ||
        !Fits(Points, growth.Points, pointPairs) || !Fits(Tags, growth.Tags, indices) ||
        !Fits(Values, growth.Values, indices) || !Fits(Keys, growth.Keys, indices) ||
        !Fits(Strings, growth.Strings, indices)) {
      return false;
    }
    Features += growth.Features;
    Rings += growth.Rings;
    Points += growth.Points;
    Tags += growth.Tags;
    Values += growth.Values;
    Keys += growth.Keys;
    Strings += growth.Strings;
    Tiles += growth.Tiles;
    return true;
  }

private:
  [[nodiscard]] static constexpr bool Fits(size_t used, size_t growth, size_t limit) noexcept {
    return used <= limit && growth <= limit - used;
  }
};

}
#endif
