#ifndef OUTSHINE_GENERATORS_FLORA_PLANTGROWTHINDEX_H
#define OUTSHINE_GENERATORS_FLORA_PLANTGROWTHINDEX_H

#include "PlantGrowth.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Generators {

class PlantGrowthIndex {
public:
  struct Hit {
    uint32_t Node = std::numeric_limits<uint32_t>::max();
    double SquaredM = std::numeric_limits<double>::infinity();

    [[nodiscard]] bool Found() const noexcept {
      return Node != std::numeric_limits<uint32_t>::max();
    }
  };

  explicit PlantGrowthIndex(size_t capacity) : Order_(capacity) {}

  void Build(std::span<const PlantGrowthPoint> points, PlantGrowthStats &stats);
  [[nodiscard]] Hit Nearest(PlantGrowthPoint point, double radiusM, PlantGrowthStats &stats) const;
  void DiscardFollowing(uint32_t first,
                        double radiusM,
                        std::span<uint8_t> free,
                        PlantGrowthStats &stats) const;

private:
  void Partition(std::span<uint32_t> order, size_t axis, PlantGrowthStats &stats);
  void Search(std::span<const uint32_t> order,
              size_t axis,
              PlantGrowthPoint point,
              Hit &hit,
              PlantGrowthStats &stats) const;
  void Discard(std::span<const uint32_t> order,
               size_t axis,
               uint32_t first,
               double squaredM,
               std::span<uint8_t> free,
               PlantGrowthStats &stats) const;
  std::span<const PlantGrowthPoint> Points_;
  std::vector<uint32_t> Order_;
};

}
#endif
