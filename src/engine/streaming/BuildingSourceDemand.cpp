#include "BuildingSourceDemand.h"
#include "StructureBuildQueue.h"
#include "SurfacePreparation.h"
#include "StructureCell.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace outshine {
namespace {
std::optional<BuildingSourceDemand::Source>
SourceOf(const StructureBuildQueue::NativeBasisQuery &query) {
  const auto &result = *query.Source;
  BuildingSourceDemand::Source source{.RequestKey = query.RequestKey,
                                      .BaseKey = result.BaseKey,
                                      .Bounds = result.Bounds,
                                      .AnchorEcef = result.Product->AnchorEcef,
                                      .Cells = 0,
                                      .HeightQualified = result.Product->HeightQualified};
  for (const auto &part : result.Product->Sources) {
    if (part.Cell == 0 || part.Cell > Generators::kStructureCellsPerTile) { return std::nullopt; }
    source.Cells |= uint64_t{1} << (part.Cell - 1u);
  }
  return source;
}

void PollSource(BuildingSourceDemand::Entry &entry,
                StructureBuildQueue &queue,
                Ground::SurfacePreparation &stack,
                uint32_t tile) {
  using Phase = BuildingSourceDemand::Phase;
  const auto query = queue.PollNativeBasis(stack, stack.Footprints(), tile);
  if (!query) {
    entry.Where = Phase::Failed;
    return;
  }
  using NativePhase = StructureBuildQueue::NativeBasisQuery::Phase;
  switch (query->Where) {
    case NativePhase::Pending: entry.Where = Phase::Pending; break;
    case NativePhase::Unavailable: entry.Where = Phase::Unrequested; break;
    case NativePhase::Missing: entry.Where = Phase::Missing; break;
    case NativePhase::Ready:
      entry.Value = SourceOf(*query);
      queue.ReleaseNativeBasis(query->RequestKey);
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
      if (entry.Where == Phase::Missing) {
        entry.Where = Phase::Unrequested;
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
