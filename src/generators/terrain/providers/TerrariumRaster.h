#ifndef OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_TERRARIUMRASTER_H
#define OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_TERRARIUMRASTER_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <scene/HeightRaster.h>

namespace outshine::Generators::Terrain {

[[nodiscard]] std::expected<HeightRaster, std::string>
DecodeTerrariumWebp(std::span<const uint8_t> bytes);

}
#endif
