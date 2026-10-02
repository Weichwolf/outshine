#ifndef OUTSHINE_ENGINE_INSTANCEPLACEMENTINPUTS_H
#define OUTSHINE_ENGINE_INSTANCEPLACEMENTINPUTS_H

#include "GroundPublication.h"
#include <scene/LevelOfDetail.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace outshine {
struct InstancePlacementInputs {
  struct Identity {
    std::weak_ptr<const void> Owner;

    [[nodiscard]] bool operator==(const Identity &other) const {
      return !Owner.owner_before(other.Owner) && !other.Owner.owner_before(Owner) &&
             Owner.lock() == other.Owner.lock();
    }
  };

  std::array<int, 3> Cell{};
  LevelOfDetail Detail = LevelOfDetail::Fine;
  uint64_t Terrain = 0;
  uint64_t Features = 0;
  uint64_t Footprints = 0;
  size_t StreetTiles = 0;
  size_t WaterTiles = 0;
  std::optional<GroundRevision> Publication;
  Identity Classes;
  Identity Table;
  Identity FeatureOrigin;

  [[nodiscard]] bool operator==(const InstancePlacementInputs &) const = default;
};
}
#endif
