#ifndef OUTSHINE_WORLD_GROUND_PUBLISHEDREGION_H
#define OUTSHINE_WORLD_GROUND_PUBLISHEDREGION_H

#include <cstddef>
#include <memory>
#include <utility>

#include "BuildingField.h"
#include "OsmField.h"
#include "StreetField.h"
#include "WaterField.h"

namespace outshine::Ground {

struct RegionSources {
  std::shared_ptr<const OsmField> Vectors;
  StreetField Ways;
  WaterField WaterBodies;

  [[nodiscard]] static RegionSources
  Snapshot(const OsmField *vectors, const StreetField &ways, const WaterField &water) {
    return {.Vectors = vectors != nullptr ? vectors->SnapshotQueries() : nullptr,
            .Ways = ways,
            .WaterBodies = water.SnapshotQueries()};
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return (Vectors ? Vectors->HeapBytes() : 0) + Ways.HeapBytes() + WaterBodies.HeapBytes();
  }
};

class PublishedRegion {
public:
  PublishedRegion(RegionSources sources, BuildingField footprints)
      : Sources_(std::move(sources)), Footprints_(std::move(footprints)) {}

  [[nodiscard]] const OsmField *Vectors() const noexcept { return Sources_.Vectors.get(); }

  [[nodiscard]] const StreetField &Ways() const noexcept { return Sources_.Ways; }

  [[nodiscard]] const WaterField &WaterBodies() const noexcept { return Sources_.WaterBodies; }

  [[nodiscard]] const BuildingField &Footprints() const noexcept { return Footprints_; }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Sources_.HeapBytes() + Footprints_.HeapBytes();
  }

private:
  RegionSources Sources_;
  BuildingField Footprints_;
};

}

#endif
