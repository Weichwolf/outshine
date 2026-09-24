#ifndef OUTSHINE_WORLD_DATA_OSMCHUNKSETLOADER_H
#define OUTSHINE_WORLD_DATA_OSMCHUNKSETLOADER_H

#include <cstddef>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "OsmElements.h"
#include <world/SourceProvider.h>

namespace outshine::Data {

struct OsmChunkSet {
  OsmElements Elements;
  std::vector<SourceCoverage> Coverage;
  size_t SourceBytes = 0;
  double ReadMs = 0.0;
  double ParseMs = 0.0;
};

class OsmChunkSetLoader {
public:
  [[nodiscard]] static std::expected<OsmChunkSet, std::string>
  Load(std::span<const SourceProvider> providers,
       std::string_view shippedRoot,
       std::stop_token stop = {});
};

}

#endif
