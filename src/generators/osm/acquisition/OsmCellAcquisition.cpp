#include "OsmCellAcquisition.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "OsmApiRegion.h"
#include <algorithm>
#include <utility>

namespace outshine::Generators::Osm {
struct CellAcquisition::Impl {
  struct Pending {
    std::unique_ptr<ApiRegion> Region;
    Data::OsmSourceChunk Chunk;
    double BeganMs = 0;
    ApiRegion::Collected Outcome = ApiRegion::Collected::Pending;
  };

  Data::SourceProvider Catalogue;
  Data::ContentStore &Store;
  Data::Transport &Wire;
  const Data::ProviderRegistry *Registry;
  std::string ShippedRoot;
  std::vector<Pending> Queries;
};

CellAcquisition::CellAcquisition(Data::SourceProvider catalogue,
                                 Data::ContentStore &store,
                                 Data::Transport &wire,
                                 const Data::ProviderRegistry *registry,
                                 std::string shippedRoot)
    : Impl_(std::make_unique<Impl>(Impl{.Catalogue = std::move(catalogue),
                                        .Store = store,
                                        .Wire = wire,
                                        .Registry = registry,
                                        .ShippedRoot = std::move(shippedRoot),
                                        .Queries = {}})) {
  Impl_->Queries.reserve(MaximumPendingCells);
}

CellAcquisition::~CellAcquisition() = default;

size_t CellAcquisition::PendingCount() const noexcept {
  return Impl_->Queries.size();
}

std::expected<void, std::string> CellAcquisition::Start(Data::GeoCellId cell) {
  auto &state = *Impl_;
  if (!cell.Valid() || state.Queries.size() == MaximumPendingCells ||
      std::ranges::any_of(state.Queries,
                          [cell](const auto &entry) { return entry.Chunk.Cell == cell; })) {
    return std::unexpected(
        "original OSM acquisition requires distinct cells within its pending limit");
  }
  auto region = ApiRegion::Create(
      state.Catalogue, state.Store, state.Wire, state.Registry, state.ShippedRoot, cell);
  if (!region) { return std::unexpected(std::move(region.error())); }
  auto chunk = (*region)->Chunk(state.Catalogue, cell);
  std::vector<Data::GeoCellId> refine;
  const auto began = state.Wire.NowMs();
  const bool cachedChildren = (*region)->Begin(state.Store, refine);
  auto outcome =
      cachedChildren
          ? std::expected<ApiRegion::Collected, std::string>(ApiRegion::Collected::Refine)
          : (*region)->Collect(chunk, began);
  if (!outcome) { return std::unexpected(std::move(outcome.error())); }
  state.Queries.push_back({.Region = std::move(*region),
                           .Chunk = std::move(chunk),
                           .BeganMs = began,
                           .Outcome = *outcome});
  return {};
}

std::expected<std::optional<SourceRead>, std::string> CellAcquisition::TakeReady() {
  auto &state = *Impl_;
  for (size_t index = 0; index < state.Queries.size(); ++index) {
    auto &entry = state.Queries[index];
    auto result = entry.Outcome == ApiRegion::Collected::Pending
                      ? entry.Region->Collect(entry.Chunk, entry.BeganMs)
                      : std::expected<ApiRegion::Collected, std::string>(entry.Outcome);
    if (!result) { return std::unexpected(std::move(result.error())); }
    if (*result == ApiRegion::Collected::Pending) { continue; }
    SourceRead ready;
    ready.ElapsedMs = state.Wire.NowMs() - entry.BeganMs;
    if (*result == ApiRegion::Collected::Refine) {
      const auto cell = entry.Chunk.Cell;
      if (!cell) { return std::unexpected("original OSM acquisition lost its cell address"); }
      ready.Refine.push_back(*cell);
    } else {
      ready.Chunks.push_back(std::move(entry.Chunk));
    }
    state.Queries.erase(state.Queries.begin() + static_cast<ptrdiff_t>(index));
    return std::optional<SourceRead>(std::move(ready));
  }
  return std::nullopt;
}
}
