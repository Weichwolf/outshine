#ifndef OUTSHINE_ENGINE_STREAMING_URBANAREATERRAINPRISMS_H
#define OUTSHINE_ENGINE_STREAMING_URBANAREATERRAINPRISMS_H

#include "UrbanAreaPrisms.h"
#include "UrbanPrismAsset.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <string>

namespace outshine {
class TerrainResidency;

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
