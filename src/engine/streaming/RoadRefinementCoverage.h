#ifndef OUTSHINE_ENGINE_STREAMING_ROADREFINEMENTCOVERAGE_H
#define OUTSHINE_ENGINE_STREAMING_ROADREFINEMENTCOVERAGE_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "OsmTransportLoader.h"
#include "TangentFrame.h"
#include "TerrainRefinement.h"

namespace outshine {

class RoadRefinementCoverage {
public:
  [[nodiscard]] static std::expected<std::vector<Generators::TerrainRefinementCorridor>,
                                     std::string>
  Build(const World::TransportNetworkSnapshot &source,
        std::span<const size_t> routeIndices,
        const TangentFrame &frame);
};

}
#endif
