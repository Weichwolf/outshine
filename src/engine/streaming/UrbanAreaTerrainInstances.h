#ifndef OUTSHINE_ENGINE_STREAMING_URBANAREATERRAININSTANCES_H
#define OUTSHINE_ENGINE_STREAMING_URBANAREATERRAININSTANCES_H

#include "UrbanAreaInstances.h"
#include <cstddef>
#include <expected>
#include <vector>

namespace outshine {
class TerrainResidency;

[[nodiscard]] std::expected<std::vector<Mat4>, UrbanInstanceError>
PrepareUrbanPatchInstances(const Generators::Osm::UrbanArea &area,
                           Generators::Osm::UrbanPatchRange patch,
                           const TerrainResidency &terrain,
                           const TangentFrame &frame,
                           size_t instancesMost);
}
#endif
