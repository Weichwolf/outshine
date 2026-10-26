#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMASSET_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMASSET_H

#include "UrbanPrismCodec.h"
#include "AssetGeneration.h"
#include <array>
#include <span>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
struct UrbanPrismAssetOptions {
  uint32_t Level = 0;
  size_t BytesMost = kUrbanPrismBytesMost;
};

[[nodiscard]] std::expected<std::string, std::string>
UrbanPrismRequestKey(std::string_view areaAssetKey,
                     const std::array<uint8_t, 32> &terrainIdentity,
                     std::span<const UrbanPatchRange> patches,
                     uint32_t level);

[[nodiscard]] std::expected<GeneratedAssetPackage, std::string> PackageUrbanPrisms(
    const UrbanPrisms &product, std::string_view request, UrbanPrismAssetOptions options = {});
}
#endif
