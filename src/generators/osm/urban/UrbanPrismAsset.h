#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMASSET_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMASSET_H

#include "UrbanPrismCodec.h"
#include "content/AssetGeneration.h"
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

[[nodiscard]] std::expected<GeneratedAssetPackage, std::string> PackageUrbanPrisms(
    const UrbanPrisms &product, std::string_view request, UrbanPrismAssetOptions options = {});
}
#endif
