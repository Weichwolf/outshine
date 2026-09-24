#ifndef OUTSHINE_ENGINE_STREAMING_ROADHEIGHTCOVERAGE_H
#define OUTSHINE_ENGINE_STREAMING_ROADHEIGHTCOVERAGE_H

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

#include "Address.h"
#include "OsmTransportLoader.h"

namespace outshine {

struct RoadHeightCoverage {
  struct Budget {
    int Zoom = 0;
    size_t MaximumEdges = 0;
    size_t MaximumTiles = 0;
  };

  std::vector<Data::TileId> Tiles;
  size_t DeferredRoutes = 0;
  size_t SelectedEdges = 0;

  [[nodiscard]] static std::expected<RoadHeightCoverage, std::string>
  Select(const World::TransportNetworkSnapshot &source, Budget budget);
};

}

#endif
