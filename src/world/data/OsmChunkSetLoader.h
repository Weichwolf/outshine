#ifndef OUTSHINE_WORLD_DATA_OSMCHUNKSETLOADER_H
#define OUTSHINE_WORLD_DATA_OSMCHUNKSETLOADER_H

#include <cstddef>
#include <expected>
#include <functional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "OsmSourceSnapshot.h"
#include <world/SourceProvider.h>

namespace outshine::Data {

struct OsmSourceChunk {
  SourceProvider Provider;
  std::string Xml;
  std::string Origin;
  double ReadMs = 0.0;
  bool FromStore = false;
};

class OsmChunkSetLoader {
public:
  using RemoteRead = std::function<std::expected<OsmSourceChunk, std::string>(
      const SourceProvider &, const std::stop_token &)>;

  [[nodiscard]] static std::expected<std::vector<OsmSourceChunk>, std::string>
  ReadRegion(std::span<const SourceProvider> providers,
             std::string_view shippedRoot,
             const std::stop_token &stop = {},
             const RemoteRead &remoteRead = {});

  [[nodiscard]] static std::expected<OsmSourceSnapshot, std::string>
  ParseRegion(std::span<const OsmSourceChunk> input, const std::stop_token &stop = {});

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
