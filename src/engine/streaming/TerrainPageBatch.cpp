#include "TerrainResidency.h"
#include "SceneRenderer.h"
#include "GroundLattice.h"
#include "VectorCapacity.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
TerrainResidency::PageBatch TerrainResidency::PlanPages(const Patchwork &patchwork,
                                                        const TangentFrame &frame,
                                                        size_t end) const {
  PageBatch batch{};
  batch.End = end;
  for (size_t at = NextSheet_; at < end; ++at) {
    const auto &sheet = patchwork.Sheets[at];
    if (sheet.Side != Render::GroundLattice::kSide ||
        sheet.Nodes.size() != Render::GroundLattice::kPageNodes) {
      ++batch.Flat;
      continue;
    }
    const auto *const found = PageIndex_.Find(sheet.Tile);
    const size_t slot = found != nullptr ? *found : Held_.size() + batch.Appended;
    const bool unchanged = found != nullptr && Renderer_->HasHeightPage(Held_[slot].Page) &&
                           std::ranges::equal(Held_[slot].Nodes, sheet.Nodes);
    if (!unchanged) {
      auto &pending = batch.Pages[batch.PageCount++];
      pending.Slot = slot;
      pending.Appended = found == nullptr;
      pending.Value = {.Tile = sheet.Tile,
                       .Nodes = std::vector<float>(sheet.Nodes.begin(), sheet.Nodes.end()),
                       .Postings = sheet.Postings,
                       .Virtual = sheet.Virtual};
      batch.Appended += static_cast<size_t>(pending.Appended);
    }
    batch.Sheets[batch.SheetCount++] = {.Tile = TileOf(sheet.Tile, {}, sheet.Nodes, frame),
                                        .Slot = slot,
                                        .Postings = sheet.Postings,
                                        .Virtual = sheet.Virtual};
  }
  return batch;
}

Render::Native::Result<void>
TerrainResidency::CommitPages(PageBatch &batch, std::span<const Render::HeightPageHandle> pages) {
  std::array<Data::TileId, Render::kHeightPageBatchMost> registered{};
  size_t inserted = 0;
  const auto rollback = [&] {
    for (size_t at = 0; at < inserted; ++at) { PageIndex_.Erase(registered[at]); }
    for (const auto page : pages) { Renderer_->ReleaseHeightPage(page); }
  };
  for (size_t at = 0; at < batch.PageCount; ++at) {
    const auto &pending = batch.Pages[at];
    if (!pending.Appended) { continue; }
    const auto indexed = PageIndex_.Emplace(pending.Value.Tile, pending.Slot);
    if (!indexed || !indexed->second) {
      rollback();
      const bool allocation = !indexed && indexed.error() == FlatMapError::AllocationFailed;
      return std::unexpected(
          Render::Native::Error{.Kind = allocation ? Render::Native::ErrorKind::OutOfMemory
                                                   : Render::Native::ErrorKind::Invalid,
                                .Message = allocation ? "height page index allocation failed"
                                                      : "height page index capacity exceeded"});
    }
    registered[inserted++] = pending.Value.Tile;
  }
  for (size_t at = 0; at < batch.PageCount; ++at) {
    auto &pending = batch.Pages[at];
    pending.Value.Page = pages[at];
    if (pending.Appended) {
      Held_.push_back(std::move(pending.Value));
    } else {
      const auto previous = Held_[pending.Slot].Page;
      Held_[pending.Slot] = std::move(pending.Value);
      Replaced_.push_back(previous);
    }
  }
  for (size_t at = 0; at < batch.SheetCount; ++at) {
    auto &pending = batch.Sheets[at];
    auto &held = Held_[pending.Slot];
    held.Postings = pending.Postings;
    held.Virtual = pending.Virtual;
    pending.Tile.Page = held.Page;
    (pending.Virtual ? Virtual_ : Instances_).push_back(pending.Tile);
  }
  Flat_ += batch.Flat;
  NextSheet_ = batch.End;
  return {};
}

Render::Native::Result<void>
TerrainResidency::UploadPages(const Patchwork &patchwork, const TangentFrame &frame, size_t end) {
  auto batch = PlanPages(patchwork, frame, end);
  GrowCapacity(Held_, Held_.size() + batch.Appended);
  GrowCapacity(Replaced_, Replaced_.size() + batch.PageCount - batch.Appended);
  const auto sheets = std::span(batch.Sheets).first(batch.SheetCount);
  const size_t virtualCount = static_cast<size_t>(
      std::ranges::count_if(sheets, [](const PendingSheet &sheet) { return sheet.Virtual; }));
  GrowCapacity(Instances_, Instances_.size() + batch.SheetCount - virtualCount);
  GrowCapacity(Virtual_, Virtual_.size() + virtualCount);
  std::array<Render::HeightPageView, Render::kHeightPageBatchMost> nodes{};
  for (size_t at = 0; at < batch.PageCount; ++at) { nodes[at] = batch.Pages[at].Value.Nodes; }
  const auto placed = Renderer_->PlaceHeightPages(std::span(nodes).first(batch.PageCount));
  if (!placed) { return std::unexpected(placed.error()); }
  return CommitPages(batch, *placed);
}
}
