#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMAPIREADER_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMAPIREADER_H

#include "RawSourceCache.h"
#include "OsmChunkSetLoader.h"
#include <SourceTransport.h>
#include <Provider.h>
#include <functional>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <GeoCellId.h>

namespace outshine::Generators::Osm {

struct SourceRead {
  std::vector<SourceChunk> Chunks;
  std::vector<GeoCellId> Refine;
  double ElapsedMs = 0.0;
};

[[nodiscard]] std::expected<SourceRead, std::string>
ReadCells(const Data::SourceProvider &catalogue,
          std::span<const GeoCellId> cells,
          Data::RawSourceCache &store,
          Data::SourceTransport &wire,
          double deadlineMs,
          const std::stop_token &stop,
          const Data::ProviderRegistry *registry = nullptr,
          std::string_view shippedRoot = {},
          const std::function<double()> &currentDeadline = {});

[[nodiscard]] std::expected<SourceRead, std::string>
ReadRegions(std::span<const Data::SourceProvider> providers,
            Data::RawSourceCache &store,
            Data::SourceTransport &wire,
            double deadlineMs,
            const std::stop_token &stop,
            const Data::ProviderRegistry *registry = nullptr,
            std::string_view shippedRoot = {},
            const std::function<double()> &currentDeadline = {});

[[nodiscard]] std::expected<SourceChunk, std::string>
ReadRegion(const Data::SourceProvider &provider,
           Data::RawSourceCache &store,
           Data::SourceTransport &wire,
           double deadlineMs,
           const std::stop_token &stop,
           const Data::ProviderRegistry *registry = nullptr,
           std::string_view shippedRoot = {});

}
#endif
