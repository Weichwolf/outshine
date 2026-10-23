#ifndef OUTSHINE_ENGINE_STREAMING_URBANAREATERRAINPRISMS_H
#define OUTSHINE_ENGINE_STREAMING_URBANAREATERRAINPRISMS_H

#include "UrbanAreaPrisms.h"
#include "UrbanPrismAsset.h"
#include <cstddef>
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <string>

namespace outshine {
class TerrainResidency;
enum class TerrainIdentityError;

struct UrbanTerrainIdentityLimits {
  size_t PatchesMost = 0, RegionsMost = 0, PagesMost = 0;
};

[[nodiscard]] std::expected<std::array<uint8_t, 32>, TerrainIdentityError>
UrbanPrismTerrainIdentity(const Generators::Osm::UrbanArea &area,
                          std::span<const Generators::Osm::UrbanPatchRange> patches,
                          const TerrainResidency &terrain,
                          UrbanTerrainIdentityLimits limits);

[[nodiscard]] std::expected<Generators::Osm::UrbanPrisms, Generators::Osm::UrbanPrismError>
PrepareUrbanPrisms(const Generators::Osm::UrbanArea &area,
                   std::span<const Generators::Osm::UrbanPatchRange> patches,
                   const TerrainResidency &terrain,
                   size_t samplesMost);

[[nodiscard]] std::expected<AssetFactory, std::string>
CaptureUrbanPrismFactory(std::shared_ptr<const Generators::Osm::UrbanArea> area,
                         std::span<const Generators::Osm::UrbanPatchRange> patches,
                         const TerrainResidency &terrain,
                         std::string request,
                         size_t samplesMost,
                         Generators::Osm::UrbanPrismAssetOptions options = {},
                         const std::stop_token &stop = {});
}
#endif
