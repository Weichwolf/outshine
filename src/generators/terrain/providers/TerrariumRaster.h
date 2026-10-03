#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRARIUMRASTER_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRARIUMRASTER_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include "TerrainGrid.h"

namespace outshine::Generators::Terrain {

[[nodiscard]] std::expected<Ground::TerrainField, std::string>
DecodeTerrariumWebp(std::span<const uint8_t> bytes);

}
#endif
