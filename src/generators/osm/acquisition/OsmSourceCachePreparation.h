#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCECACHEPREPARATION_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCECACHEPREPARATION_H

#include "OsmSourceAcquisition.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
struct SourceCacheRequest {
  const Data::SourceProvider &Catalogue;
  std::span<const Data::GeoCellId> Cells;
  SourceAcquisition::CellLimits Limits;
  std::string_view ShippedRoot;
  std::string CacheDirectory;
  double BudgetS = 0;
};

struct SourceCacheProgress {
  size_t ValidatedCells = 0;
  size_t RequiredCells = 0;
  double ElapsedS = 0;
};

[[nodiscard]] std::expected<void, std::string>
PrepareSourceCache(const SourceCacheRequest &request,
                   SourceAcquisition::Workers workers,
                   Data::Transport &wire,
                   const Data::ProviderRegistry &registry,
                   const std::function<void(SourceCacheProgress)> &progress = {});
}
#endif
