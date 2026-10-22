#ifndef OUTSHINE_ENGINE_STREAMING_URBANAREATERRAINPRISMS_H
#define OUTSHINE_ENGINE_STREAMING_URBANAREATERRAINPRISMS_H

#include "UrbanAreaPrisms.h"
#include <cstddef>
#include <expected>
#include <span>

namespace outshine {
class TerrainResidency;

[[nodiscard]] std::expected<Generators::Osm::UrbanPrisms, Generators::Osm::UrbanPrismError>
PrepareUrbanPrisms(const Generators::Osm::UrbanArea &area,
                   std::span<const Generators::Osm::UrbanPatchRange> patches,
                   const TerrainResidency &terrain,
                   size_t samplesMost);
}
#endif
