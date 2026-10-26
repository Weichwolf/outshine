#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEASSET_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEASSET_H

#include "Earth.h"
#include "RoadPageGeometry.h"
#include "RoadPageAppearance.h"
#include "Box.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct RoadPageAddress {
  std::string PlanKey;
  std::string MaterialKey;
  uint32_t MaterialIndex = 0;
  LongitudeLatitude Anchor;
  uint32_t Level = 0;
  std::vector<RoadPagePart> Parts;
  RoadPageAppearance Appearance = LegacyRoadPageV1{};
};

struct RoadPageAsset {
  RoadPageAddress Address;
  RoadPageGeometry Geometry;
};

[[nodiscard]] std::string RoadPageKey(const RoadPageAddress &address);
[[nodiscard]] std::expected<std::vector<uint8_t>, std::string_view>
EncodeRoadPageAsset(const RoadPageAsset &page, size_t bytesMost);
[[nodiscard]] std::optional<RoadPageAsset> DecodeRoadPageAsset(std::span<const uint8_t> bytes,
                                                               size_t bytesMost);
[[nodiscard]] Box RoadPageBoundsEcef(const RoadPageAsset &page);
}
#endif
