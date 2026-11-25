#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPRECISION_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPRECISION_H

#include "TerrainPatchwork.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators::Osm {

struct GroundTerrainPrecision {
  std::vector<uint8_t> Base;
  std::vector<uint8_t> Residual;
  double ErrorM = 0;
};

[[nodiscard]] std::optional<GroundTerrainPrecision> EncodeGroundTerrainPrecision(
    std::span<const Sheet> sheets, std::span<const uint32_t> slots, size_t bytesMost);

[[nodiscard]] std::optional<std::vector<Sheet>>
DecodeGroundTerrainBase(std::span<const uint8_t> bytes, size_t expected);

[[nodiscard]] bool ApplyGroundTerrainResidual(std::span<const uint8_t> bytes,
                                              std::span<Sheet> sheets);
}
#endif
