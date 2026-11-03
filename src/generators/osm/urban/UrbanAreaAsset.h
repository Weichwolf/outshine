#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANAREAASSET_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANAREAASSET_H

#include "UrbanAreaCodec.h"
#include "AssetPackage.h"
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
[[nodiscard]] std::expected<AssetPackage, std::string> PackageUrbanArea(
    const UrbanArea &area, std::string_view request, size_t bytesMost = kUrbanAreaBytesMost);
}
#endif
