#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINDEFORMATION_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINDEFORMATION_H

#include "TerrainPress.h"
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
inline constexpr size_t kTerrainDeformationBytesMost = size_t{64} * 1024 * 1024;

struct PreparedTerrainDeformation {
  std::vector<Sheet> Pages;
  PressedTerrain Effects;
};

[[nodiscard]] std::expected<std::string, std::string>
TerrainDeformationKey(const Patchwork &input,
                      std::span<const EarthworkStamp> stamps,
                      const TangentFrame &frame,
                      TerrainPageLayout layout,
                      double mostEarthworkM);
[[nodiscard]] std::optional<std::vector<uint8_t>> EncodeTerrainDeformation(
    const std::string &key, std::span<const Sheet> pages, const PressedTerrain &measures);
[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeTerrainDeformation(const std::string &key, const PreparedTerrainDeformation &product);
[[nodiscard]] std::optional<PreparedTerrainDeformation>
DecodeTerrainDeformation(const std::string &key, std::span<const uint8_t> bytes);
}

#endif
