#ifndef OUTSHINE_GENERATORS_OSM_OSMAPIREADER_H
#define OUTSHINE_GENERATORS_OSM_OSMAPIREADER_H

#include "ContentStore.h"
#include "OsmChunkSetLoader.h"
#include <world/data/Transport.h>
#include <world/Provider.h>
#include <functional>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <world/data/GeoCellId.h>

namespace outshine::Generators::Osm {

struct SourceRead {
  std::vector<Data::OsmSourceChunk> Chunks;
  std::vector<Data::GeoCellId> Refine;
  double ElapsedMs = 0.0;
};

[[nodiscard]] std::expected<SourceRead, std::string>
ReadCells(const Data::SourceProvider &catalogue,
          std::span<const Data::GeoCellId> cells,
          Data::ContentStore &store,
          Data::Transport &wire,
          double deadlineMs,
          const std::stop_token &stop,
          const Data::ProviderRegistry *registry = nullptr,
          std::string_view shippedRoot = {},
          const std::function<double()> &currentDeadline = {});

[[nodiscard]] std::expected<SourceRead, std::string>
ReadRegions(std::span<const Data::SourceProvider> providers,
            Data::ContentStore &store,
            Data::Transport &wire,
            double deadlineMs,
            const std::stop_token &stop,
            const Data::ProviderRegistry *registry = nullptr,
            std::string_view shippedRoot = {},
            const std::function<double()> &currentDeadline = {});

[[nodiscard]] std::expected<Data::OsmSourceChunk, std::string>
ReadRegion(const Data::SourceProvider &provider,
           Data::ContentStore &store,
           Data::Transport &wire,
           double deadlineMs,
           const std::stop_token &stop,
           const Data::ProviderRegistry *registry = nullptr,
           std::string_view shippedRoot = {});

}
#endif
