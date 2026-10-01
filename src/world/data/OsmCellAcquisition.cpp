#include "OsmCellAcquisition.h"
#include "OsmApiRegion.h"
#include <algorithm>
#include <utility>

namespace outshine::Data {
struct OsmCellAcquisition::Impl {
  struct Pending {
    std::unique_ptr<OsmApiRegion> Region;
    OsmSourceChunk Chunk;
    double BeganMs = 0;
    bool Refine = false;
  };

  SourceProvider Catalogue;
  ContentStore &Store;
  Transport &Wire;
  const ProviderRegistry *Registry;
  std::string ShippedRoot;
  std::vector<Pending> Queries;
};

OsmCellAcquisition::OsmCellAcquisition(SourceProvider catalogue,
                                       ContentStore &store,
                                       Transport &wire,
                                       const ProviderRegistry *registry,
                                       std::string shippedRoot)
    : Impl_(std::make_unique<Impl>(Impl{.Catalogue = std::move(catalogue),
                                        .Store = store,
                                        .Wire = wire,
                                        .Registry = registry,
                                        .ShippedRoot = std::move(shippedRoot),
                                        .Queries = {}})) {
  Impl_->Queries.reserve(MaximumPendingCells);
}

OsmCellAcquisition::~OsmCellAcquisition() = default;

size_t OsmCellAcquisition::PendingCount() const noexcept {
  return Impl_->Queries.size();
}

std::expected<void, std::string> OsmCellAcquisition::Start(GeoCellId cell) {
  auto &state = *Impl_;
  if (!cell.Valid() || state.Queries.size() == MaximumPendingCells ||
      std::ranges::any_of(state.Queries,
                          [cell](const auto &entry) { return entry.Chunk.Cell == cell; })) {
    return std::unexpected(
        "original OSM acquisition requires distinct cells within its pending limit");
  }
  auto region = OsmApiRegion::Create(
      state.Catalogue, state.Store, state.Wire, state.Registry, state.ShippedRoot, cell);
  if (!region) { return std::unexpected(std::move(region.error())); }
  auto chunk = (*region)->Chunk(state.Catalogue, cell);
  std::vector<GeoCellId> refine;
  const auto began = state.Wire.NowMs();
  const bool cachedChildren = (*region)->Begin(state.Store, refine);
  state.Queries.push_back({.Region = std::move(*region),
                           .Chunk = std::move(chunk),
                           .BeganMs = began,
                           .Refine = cachedChildren});
  return {};
}

std::expected<std::optional<OsmSourceRead>, std::string> OsmCellAcquisition::TakeReady() {
  auto &state = *Impl_;
  for (size_t index = 0; index < state.Queries.size(); ++index) {
    auto &entry = state.Queries[index];
    auto result =
        entry.Refine
            ? std::expected<OsmApiRegion::Collected, std::string>(OsmApiRegion::Collected::Refine)
            : entry.Region->Collect(entry.Chunk, entry.BeganMs);
    if (!result) { return std::unexpected(std::move(result.error())); }
    if (*result == OsmApiRegion::Collected::Pending) { continue; }
    OsmSourceRead ready;
    ready.ElapsedMs = state.Wire.NowMs() - entry.BeganMs;
    if (*result == OsmApiRegion::Collected::Refine) {
      ready.Refine.push_back(*entry.Chunk.Cell);
    } else {
      ready.Chunks.push_back(std::move(entry.Chunk));
    }
    state.Queries.erase(state.Queries.begin() + static_cast<ptrdiff_t>(index));
    return std::optional<OsmSourceRead>(std::move(ready));
  }
  return std::nullopt;
}
}
