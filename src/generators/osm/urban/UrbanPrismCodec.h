#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMCODEC_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMCODEC_H

#include "UrbanAreaPrisms.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators::Osm {
inline constexpr uint64_t kUrbanPrismFormat = 0x31534d535250534fULL;
inline constexpr size_t kUrbanPrismBytesMost = size_t{16} * 1024 * 1024;

[[nodiscard]] std::expected<std::vector<uint8_t>, std::string>
EncodeUrbanPrisms(const UrbanPrisms &product, size_t bytesMost = kUrbanPrismBytesMost);

[[nodiscard]] std::expected<UrbanPrisms, std::string>
DecodeUrbanPrisms(std::span<const uint8_t> bytes, size_t bytesMost = kUrbanPrismBytesMost);
}
#endif
