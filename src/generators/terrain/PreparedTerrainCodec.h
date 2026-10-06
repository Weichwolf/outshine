#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINCODEC_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINCODEC_H

#include "TerrainGrid.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {

inline constexpr size_t kPreparedTerrainBytesMost = size_t{64} * 1024u * 1024u;

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodePreparedTerrain(Data::TileId at, const ::outshine::Ground::TerrainField &field);

[[nodiscard]] std::optional<::outshine::Ground::TerrainField>
DecodePreparedTerrain(Data::TileId at, std::span<const uint8_t> bytes, size_t residentBytesMost);

}
#endif
