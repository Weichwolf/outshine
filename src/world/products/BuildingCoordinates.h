#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGCOORDINATES_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGCOORDINATES_H

#include "Capacity.h"
#include "GeographicRing.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Ground {
class BuildingCoordinates {
public:
  BuildingCoordinates() = default;
  ~BuildingCoordinates() = default;
  BuildingCoordinates(const BuildingCoordinates &) = default;
  BuildingCoordinates &operator=(const BuildingCoordinates &) = default;

  BuildingCoordinates(BuildingCoordinates &&other) noexcept { *this = std::move(other); }

  BuildingCoordinates &operator=(BuildingCoordinates &&other) noexcept {
    if (this == &other) { return *this; }
    Points_ = std::exchange(other.Points_, {});
    Rings_ = std::exchange(other.Rings_, {});
    Owner_ = std::move(other.Owner_);
    BackingBytes_ = std::exchange(other.BackingBytes_, 0);
    return *this;
  }

  void Own(std::vector<double> points, std::vector<GeographicRing> rings) {
    const auto storage =
        std::make_shared<Storage>(Storage{.Points = std::move(points), .Rings = std::move(rings)});
    Share(storage,
          storage->Points,
          storage->Rings,
          CapacityBytes(storage->Points) + CapacityBytes(storage->Rings));
  }

  void Share(std::shared_ptr<const void> owner,
             std::span<const double> points,
             std::span<const GeographicRing> rings,
             size_t retainedBytes) {
    assert(owner != nullptr);
    Owner_ = std::move(owner);
    Points_ = points;
    Rings_ = rings;
    BackingBytes_ = retainedBytes;
  }

  [[nodiscard]] std::span<const double> Points() const noexcept { return Points_; }

  [[nodiscard]] std::span<const GeographicRing> Rings() const noexcept { return Rings_; }

  [[nodiscard]] size_t HeapBytes() const noexcept { return BackingBytes_; }

private:
  struct Storage {
    std::vector<double> Points;
    std::vector<GeographicRing> Rings;
  };

  std::shared_ptr<const void> Owner_;
  std::span<const double> Points_;
  std::span<const GeographicRing> Rings_;
  size_t BackingBytes_ = 0;
};
}

#endif
