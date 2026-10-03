#ifndef OUTSHINE_GENERATORS_OSM_IMPORT_OSMCHUNKSETLOADER_H
#define OUTSHINE_GENERATORS_OSM_IMPORT_OSMCHUNKSETLOADER_H

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "OsmSourceSnapshot.h"
#include <world/SourceProvider.h>

namespace outshine::Generators::Osm {

struct SourceChunk {
  Data::SourceProvider Provider;
  std::string Xml;
  std::string Origin;
  double ReadMs = 0.0;
  bool FromStore = false;
  std::optional<Data::GeoCellId> Cell = std::nullopt;
};

class ChunkSetLoader {
public:
  using RemoteRead = std::function<std::expected<SourceChunk, std::string>(
      const Data::SourceProvider &, const std::stop_token &)>;

  [[nodiscard]] static std::expected<std::vector<SourceChunk>, std::string>
  ReadRegion(std::span<const Data::SourceProvider> providers,
             std::string_view shippedRoot,
             const std::stop_token &stop = {},
             const RemoteRead &remoteRead = {});

  [[nodiscard]] static std::expected<SourceSnapshot, std::string>
  ParseRegion(std::span<const SourceChunk> input, const std::stop_token &stop = {});

  [[nodiscard]] static std::expected<SourceSnapshot, std::string>
  ParseCell(const SourceChunk &input, const std::stop_token &stop = {});

  [[nodiscard]] static std::expected<SourceSnapshot, std::string>
  LoadRegion(std::span<const Data::SourceProvider> providers,
             std::string_view shippedRoot,
             const std::stop_token &stop = {});

  [[nodiscard]] static std::expected<SourceSnapshot, std::string>
  Load(std::span<const Data::SourceProvider> providers,
       std::string_view shippedRoot,
       const std::stop_token &stop = {});

private:
  [[nodiscard]] static std::expected<SourceSnapshot, std::string>
  ParseChunks(std::span<const SourceChunk> input, const std::stop_token &stop);
};

}

#endif
