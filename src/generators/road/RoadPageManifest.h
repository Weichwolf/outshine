#ifndef OUTSHINE_GENERATORS_ROAD_ROADPAGEMANIFEST_H
#define OUTSHINE_GENERATORS_ROAD_ROADPAGEMANIFEST_H

#include "Earth.h"
#include "RoadPageAsset.h"
#include "Box.h"
#include "Geometry.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
struct RoadPageReference {
  std::string Key;
  Box Bounds;
  uint64_t ByteCount = 0;
  uint32_t Level = 0;
  std::vector<RoadPagePart> Parts;
};

struct RoadPageManifest {
  std::string PlanKey;
  std::string MaterialKey;
  LongitudeLatitude Anchor;
  Geometry Materials;
  std::vector<RoadPageReference> Pages;
};

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeRoadPageManifest(const RoadPageManifest &manifest, size_t bytesMost);
[[nodiscard]] std::optional<RoadPageManifest> DecodeRoadPageManifest(std::span<const uint8_t> bytes,
                                                                     size_t bytesMost);
[[nodiscard]] std::string RoadPageManifestKey(const RoadPageManifest &manifest, size_t bytesMost);
[[nodiscard]] Box RoadPageManifestBounds(const RoadPageManifest &manifest);
}
#endif
