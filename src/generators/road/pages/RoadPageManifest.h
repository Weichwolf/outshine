#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEMANIFEST_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEMANIFEST_H

#include "Earth.h"
#include "RoadPageAsset.h"
#include "Box.h"
#include "Geometry.h"
#include <memory>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {
struct PageIndexAsset;
}

namespace outshine::Generators {
struct RoadPageReference {
  std::string Key;
  Box Bounds;
  uint64_t ByteCount = 0;
  uint32_t Level = 0;
  std::vector<RoadPagePart> Parts;
  RoadPageAppearance Appearance = LegacyRoadPageV1{};
  std::optional<double> PositionErrorM = std::nullopt;
  RoadDisplayPolicy DisplayPolicy = RoadDisplayPolicy::UpperOnly;
  bool Clustered = false;
};

struct RoadPageManifest {
  std::string PlanKey;
  std::string MaterialKey;
  LongitudeLatitude Anchor;
  Geometry Materials;
  std::vector<RoadPageReference> Pages;
  std::shared_ptr<const PageIndexAsset> PageIndex;
  RoadPageAppearance Appearance = LegacyRoadPageV1{};
};

[[nodiscard]] std::string RoadPageManifestAppearanceKey(std::span<const RoadPageReference> pages);

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeRoadPageManifest(const RoadPageManifest &manifest, size_t bytesMost);
[[nodiscard]] std::optional<RoadPageManifest> DecodeRoadPageManifest(std::span<const uint8_t> bytes,
                                                                     size_t bytesMost);
[[nodiscard]] std::string RoadPageManifestKey(const RoadPageManifest &manifest, size_t bytesMost);
[[nodiscard]] Box RoadPageManifestBounds(const RoadPageManifest &manifest);
}
#endif
