#include "OsmSourceCachePreparation.h"

#include <chrono>
#include <expected>
#include <functional>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
constexpr double kPollingSliceS = 0.005;
}

std::expected<void, std::string>
PrepareSourceCache(const SourceCacheRequest &request,
                   SourceAcquisition::Workers workers,
                   Data::Transport &wire,
                   const Data::ProviderRegistry &registry,
                   const std::function<void(SourceCacheProgress)> &progress) {
  SourceAcquisition source(
      workers, &wire, request.CacheDirectory, SourceAcquisition::Target::Cache);
  if (auto budget = source.SetAcquisitionBudget(request.BudgetS); !budget) {
    return std::unexpected(std::move(budget.error()));
  }
  const auto began = std::chrono::steady_clock::now();
  if (auto requested = source.RequestCells(
          request.Catalogue, request.Cells, request.Limits, request.ShippedRoot, &registry);
      !requested) {
    return std::unexpected(std::move(requested.error()));
  }
  double last = -5;
  for (;;) {
    source.Poll();
    const auto phase = source.CurrentPhase();
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const bool ready =
        phase == SourceAcquisition::Phase::Ready || phase == SourceAcquisition::Phase::Inactive;
    if (progress && (elapsed - last >= 5 || ready)) {
      progress({.ValidatedCells = source.PreparedCellCount(),
                .RequiredCells = source.RequiredCellCount(),
                .ElapsedS = elapsed});
      last = elapsed;
    }
    if (phase == SourceAcquisition::Phase::Failed) {
      return std::unexpected(std::string(source.Error()));
    }
    if (elapsed >= request.BudgetS) {
      return std::unexpected("original OSM source cache preparation deadline exceeded");
    }
    if (ready) { return {}; }
    (void)source.AwaitSlice(kPollingSliceS);
  }
}

}
