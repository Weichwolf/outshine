#include "BuildingSourceDemand.h"
#include "StructureBuildQueue.h"
#include "SurfacePreparation.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace outshine {
namespace {
std::optional<BuildingSourceDemand::Source>
SourceOf(const StructureBuildQueue::NativeAssetQuery &query) {
  const auto *result = query.Metadata();
  if (result == nullptr) { return std::nullopt; }
  BuildingSourceDemand::Source source{.RequestKey = query.RequestKey,
                                      .BaseKey = result->BaseKey,
                                      .Bounds = result->Bounds,
                                      .AnchorEcef = result->Metadata.AnchorEcef,
                                      .Cells = result->Metadata.Cells,
                                      .HeightQualified = result->Metadata.HeightQualified,
                                      .CellSourceCounts = result->Metadata.CellSourceCounts};
  return source;
}

void PollSource(BuildingSourceDemand::Entry &entry,
                StructureBuildQueue &queue,
                Ground::SurfacePreparation &stack,
                uint32_t tile) {
  using Phase = BuildingSourceDemand::Phase;
  const auto query =
      queue.PollNativeAsset(stack,
                            stack.Footprints(),
                            tile,
                            StructureBuildQueue::NativeAssetQuery::Product::SourceMetadata);
  if (!query) {
    entry.Where = Phase::Failed;
    return;
  }
  using NativePhase = StructureBuildQueue::NativeAssetQuery::Phase;
  switch (query->Where) {
    case NativePhase::Pending: entry.Where = Phase::Pending; break;
    case NativePhase::Unavailable: entry.Where = Phase::Unrequested; break;
    case NativePhase::Missing: entry.Where = Phase::Missing; break;
    case NativePhase::Ready:
      entry.Value = SourceOf(*query);
      queue.ReleaseNativeAsset(query->RequestKey,
                               StructureBuildQueue::NativeAssetQuery::Product::SourceMetadata);
      entry.Where = entry.Value ? Phase::Ready : Phase::Failed;
      break;
  }
}

}

bool BuildingSourceDemand::Advance(StructureBuildQueue &queue,
                                   Ground::SurfacePreparation &stack,
                                   size_t requestsMost) {
  const auto &assets = stack.BuildingAssets();
  const auto *vectors = stack.Vectors();
  if (!assets || vectors == nullptr) {
    if (Entries_.empty()) { return false; }
    Entries_.clear();
    Assets_.reset();
    Vectors_ = nullptr;
    Next_ = 0;
    return true;
  }
  bool changed = false;
  const auto shape = stack.Pool().Shaped();
  const auto generation = vectors->Generation();
  const double span = stack.Footprints().TileSpanM();
  const auto writes = assets->Costs().BasisWrites;
  if (Assets_.lock() != assets || Vectors_ != vectors || VectorGeneration_ != generation ||
      Shape_ != shape || TileSpanM_ != span || Entries_.size() != vectors->Tiles().size()) {
    Entries_.clear();
    Entries_.resize(vectors->Tiles().size());
    Assets_ = assets;
    Vectors_ = vectors;
    VectorGeneration_ = generation;
    Shape_ = shape;
    TileSpanM_ = span;
    Next_ = 0;
    changed = true;
  } else if (BasisWrites_ != writes) {
    for (auto &entry : Entries_) {
      if (entry.Where != Phase::Unrequested && entry.Where != Phase::Pending) {
        entry = {};
        changed = true;
      }
    }
  }
  BasisWrites_ = writes;
  if (!vectors->SettledWithin(0) || !stack.Footprints().Anchored()) { return changed; }
  for (size_t checked = 0; checked < std::min(requestsMost, Entries_.size()); ++checked) {
    const auto tile = static_cast<uint32_t>(Next_);
    auto &entry = Entries_[Next_];
    Next_ = (Next_ + 1u) % Entries_.size();
    if (entry.Where != Phase::Unrequested && entry.Where != Phase::Pending) { continue; }
    const auto before = entry.Where;
    PollSource(entry, queue, stack, tile);
    changed |= before != entry.Where;
  }
  return changed;
}

BuildingSourceDemand::Summary BuildingSourceDemand::Counts() const noexcept {
  Summary result;
  for (const auto &entry : Entries_) {
    switch (entry.Where) {
      case Phase::Ready: ++result.Ready; break;
      case Phase::Missing: ++result.Missing; break;
      case Phase::Failed: ++result.Failed; break;
      case Phase::Unrequested:
      case Phase::Pending: ++result.Pending; break;
    }
  }
  return result;
}

size_t BuildingSourceDemand::HeapBytes() const noexcept {
  size_t bytes = Entries_.capacity() * sizeof(Entry);
  for (const auto &entry : Entries_) {
    if (entry.Value) {
      bytes += entry.Value->RequestKey.capacity() + entry.Value->BaseKey.capacity();
    }
  }
  return bytes;
}
}
