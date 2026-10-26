#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANAREACODEC_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANAREACODEC_H

#include "UrbanArea.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators::Osm {
inline constexpr size_t kUrbanAreaBytesMost = size_t{8} * 1024 * 1024;

[[nodiscard]] std::expected<std::vector<uint8_t>, std::string>
EncodeUrbanArea(const UrbanArea &area, size_t bytesMost = kUrbanAreaBytesMost);

[[nodiscard]] std::expected<UrbanArea, std::string>
DecodeUrbanArea(std::span<const uint8_t> bytes, size_t bytesMost = kUrbanAreaBytesMost);
}
#endif
