#ifndef OUTSHINE_WORLD_DATA_OSMCHUNKSETLOADER_H
#define OUTSHINE_WORLD_DATA_OSMCHUNKSETLOADER_H

#include <cstddef>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "OsmSourceSnapshot.h"
#include <world/SourceProvider.h>

namespace outshine::Data {

class OsmChunkSetLoader {
public:
  [[nodiscard]] static std::expected<OsmSourceSnapshot, std::string>
  LoadRegion(std::span<const SourceProvider> providers,
             std::string_view shippedRoot,
             const std::stop_token &stop = {});

  [[nodiscard]] static std::expected<OsmSourceSnapshot, std::string>
  Load(std::span<const SourceProvider> providers,
       std::string_view shippedRoot,
       const std::stop_token &stop = {});
};

}

#endif
