#ifndef OUTSHINE_WORLD_GROUND_GROUNDREGIONASSET_H
#define OUTSHINE_WORLD_GROUND_GROUNDREGIONASSET_H

#include "ClassStructure.h"
#include "GroundMesher.h"
#include "Wayfinding.h"
#include "scene/Geometry.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
struct GroundRegionAsset {
  LongitudeLatitude Anchor;
  Patchwork Terrain;
  Geometry Surfaces;
  std::shared_ptr<const ClassStructure> Classes;
  std::vector<float> ClassPalette;
  std::shared_ptr<const Path::Network> Network;
  uint64_t NetworkWays = 0;
  int GroundSurface = -1;
  Vec3 GroundAlbedo;
  uint64_t MissingRims = 0;
};

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeGroundRegionAsset(const GroundRegionAsset &region, size_t bytesMost);
[[nodiscard]] std::optional<GroundRegionAsset>
DecodeGroundRegionAsset(std::span<const uint8_t> bytes, size_t bytesMost);
[[nodiscard]] Box GroundRegionBoundsEcef(const GroundRegionAsset &region);
}
#endif
