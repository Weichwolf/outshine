#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINDEFORMATIONPARTS_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINDEFORMATIONPARTS_H

#include "PreparedTerrainDeformation.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
struct TerrainDeformationParts {
  uint64_t HeapBytes = 0;
  PressedTerrain Effects;
  std::vector<uint32_t> PageCounts;
};

[[nodiscard]] size_t TerrainDeformationHeap(std::span<const Sheet> pages) noexcept;
[[nodiscard]] std::optional<std::vector<uint32_t>>
TerrainDeformationPageCounts(std::span<const Sheet> pages);
[[nodiscard]] std::string TerrainDeformationPartKey(const std::string &key, size_t part);
[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeTerrainDeformationParts(const std::string &key, const TerrainDeformationParts &parts);
[[nodiscard]] std::optional<TerrainDeformationParts>
DecodeTerrainDeformationParts(const std::string &key, std::span<const uint8_t> bytes);
}

#endif
