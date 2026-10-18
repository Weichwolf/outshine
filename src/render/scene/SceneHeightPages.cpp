#include "SceneResources.h"
#include "SubjectDraw.h"
#include "TerrainTileUpload.h"
#include "VectorCapacity.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
Native::Result<std::vector<HeightPageHandle>>
SceneResources::PlaceHeightPages(SubjectDraw &subjects, std::span<const HeightPageView> nodes) {
  if (nodes.size() > kHeightPageBatchMost) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "height-page batch exceeds its budget"});
  }
  if (nodes.empty()) { return std::vector<HeightPageHandle>{}; }
  size_t free = 0;
  for (uint32_t slot = FirstFreeHeightPage_; slot != kNoResourceSlot && free < nodes.size();
       slot = HeightPages_[slot].State.NextFree) {
    ++free;
  }
  const size_t appended = nodes.size() - free;
  if (appended > kNoResourceSlot - HeightPages_.size()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "height-page storage exceeds the native slot index range"});
  }
  GrowCapacity(HeightPages_, HeightPages_.size() + appended);
  std::array<HeightPage, kHeightPageBatchMost> pending{};
  std::array<HeightPageView, kHeightPageBatchMost> views{};
  std::vector<HeightPageHandle> handles;
  handles.reserve(nodes.size());
  for (size_t at = 0; at < nodes.size(); ++at) {
    pending[at].Nodes =
        std::make_shared<const std::vector<float>>(nodes[at].begin(), nodes[at].end());
    views[at] = *pending[at].Nodes;
  }
  const auto placed = subjects.Ground().PlacePages(std::span(views).first(nodes.size()));
  if (!placed) { return std::unexpected(placed.error()); }
  for (size_t at = 0; at < nodes.size(); ++at) {
    auto &held = pending[at];
    held.Resident = (*placed)[at];
    held.State.Occupied = true;
    uint32_t slot = FirstFreeHeightPage_;
    if (slot != kNoResourceSlot) {
      held.State.Generation = HeightPages_[slot].State.Generation;
      FirstFreeHeightPage_ = HeightPages_[slot].State.NextFree;
      HeightPages_[slot] = std::move(held);
    } else {
      slot = static_cast<uint32_t>(HeightPages_.size());
      HeightPages_.push_back(std::move(held));
    }
    handles.push_back({.Slot = slot, .Generation = HeightPages_[slot].State.Generation});
  }
  return handles;
}

Native::Result<HeightPageHandle> SceneResources::PlaceHeightPage(SubjectDraw &subjects,
                                                                 std::span<const float> nodes) {
  const std::array batch{nodes};
  auto placed = PlaceHeightPages(subjects, batch);
  if (!placed) { return std::unexpected(placed.error()); }
  return placed->front();
}

bool SceneResources::HasHeightPage(HeightPageHandle handle) const noexcept {
  return handle.Slot < HeightPages_.size() &&
         HeightPages_[handle.Slot].State.Matches(handle.Generation);
}

void SceneResources::ReleaseHeightPage(SubjectDraw &subjects, HeightPageHandle which) {
  if (!HasHeightPage(which)) { return; }
  HeightPage &page = HeightPages_[which.Slot];
  subjects.Ground().ReleasePage(page.Resident);
  ResourceSlotState state = page.State;
  if (state.Release()) {
    state.NextFree = FirstFreeHeightPage_;
    FirstFreeHeightPage_ = which.Slot;
  }
  page = HeightPage{};
  page.State = state;
}

PageId SceneResources::HeightPageResident(HeightPageHandle which) const noexcept {
  return HasHeightPage(which) ? HeightPages_[which.Slot].Resident : kNoPage;
}

Native::Result<size_t>
SceneResources::RestoreHeightPageBatch(SubjectDraw &subjects, size_t first, size_t pagesMost) {
  const size_t end =
      first + std::min({pagesMost, kHeightPageBatchMost, HeightPages_.size() - first});
  std::array<HeightPageView, kHeightPageBatchMost> nodes{};
  std::array<size_t, kHeightPageBatchMost> slots{};
  size_t count = 0;
  for (size_t at = first; at < end; ++at) {
    const auto &page = HeightPages_[at];
    if (!page.State.Occupied) { continue; }
    nodes[count] = *page.Nodes;
    slots[count++] = at;
  }
  const auto placed = subjects.Ground().PlacePages(std::span(nodes).first(count));
  if (!placed) { return std::unexpected(placed.error()); }
  for (size_t at = 0; at < count; ++at) { HeightPages_[slots[at]].Resident = (*placed)[at]; }
  return end;
}

bool SceneResources::RestoreHeightPages(SubjectDraw &subjects, std::string &error) {
  size_t next = 0;
  while (next < HeightPages_.size()) {
    auto advanced = RestoreHeightPageBatch(subjects, next, kHeightPageBatchMost);
    if (!advanced) {
      error = std::move(advanced.error().Message);
      return false;
    }
    next = *advanced;
  }
  return true;
}

Native::Result<void> SceneResources::SetGroundGrid(SubjectDraw &subjects,
                                                   std::span<const float> fractions) {
  const auto uploaded = subjects.Ground().SetGrid(fractions);
  if (!uploaded) { return uploaded; }
  GroundGrid_.assign(fractions.begin(), fractions.end());
  return {};
}

Native::Result<void> SceneResources::SetTerrainTiles(SubjectDraw &subjects,
                                                     std::span<const TerrainTile> real,
                                                     std::span<const TerrainTile> virtual_) {
  const auto translate =
      [this](std::span<const TerrainTile> source) -> Native::Result<std::vector<GroundTile>> {
    std::vector<GroundTile> into;
    into.reserve(source.size());
    for (const TerrainTile &tile : source) {
      const auto encoded = EncodeTerrainTile(tile, HeightPageResident(tile.Page));
      if (!encoded) {
        return std::unexpected(Native::Error{
            .Kind = Native::ErrorKind::Invalid,
            .Message = std::string(encoded.error()) + ": slot=" + std::to_string(tile.Page.Slot) +
                       ", generation=" + std::to_string(tile.Page.Generation)});
      }
      into.push_back(*encoded);
    }
    return into;
  };
  const auto residentReal = translate(real);
  if (!residentReal) { return std::unexpected(residentReal.error()); }
  const auto residentVirtual = translate(virtual_);
  if (!residentVirtual) { return std::unexpected(residentVirtual.error()); }
  const auto uploaded = subjects.Ground().SetInstances(*residentReal, *residentVirtual);
  if (!uploaded) { return uploaded; }
  if (real.data() != GroundReal_.data()) {
    GroundReal_.assign(real.begin(), real.end());
  } else {
    GroundReal_.resize(real.size());
  }
  if (virtual_.data() != GroundVirtual_.data()) {
    GroundVirtual_.assign(virtual_.begin(), virtual_.end());
  } else {
    GroundVirtual_.resize(virtual_.size());
  }
  return {};
}

bool SceneResources::RestoreTerrain(SubjectDraw &subjects, std::string &error) {
  size_t nextPage = 0;
  for (;;) {
    const size_t previous = nextPage;
    auto restored = AdvanceTerrainRestore(subjects, nextPage, kHeightPageBatchMost);
    if (!restored) {
      error = std::move(restored.error());
      return false;
    }
    if (*restored) { return true; }
    if (nextPage == previous) {
      error = "terrain restoration is waiting for GPU upload";
      return false;
    }
  }
}

std::expected<bool, std::string>
SceneResources::AdvanceTerrainRestore(SubjectDraw &subjects, size_t &nextPage, size_t pagesMost) {
  assert(nextPage <= HeightPages_.size());
  auto advanced = RestoreHeightPageBatch(subjects, nextPage, pagesMost);
  if (!advanced) {
    if (advanced.error().Kind == Native::ErrorKind::Busy) { return false; }
    return std::unexpected(std::move(advanced.error().Message));
  }
  nextPage = *advanced;
  if (nextPage < HeightPages_.size()) { return false; }
  if (!GroundGrid_.empty()) {
    const auto uploaded = subjects.Ground().SetGrid(GroundGrid_);
    if (!uploaded) { return std::unexpected(uploaded.error().Message); }
  }
  const auto restored = SetTerrainTiles(subjects, GroundReal_, GroundVirtual_);
  if (!restored) { return std::unexpected(restored.error().Message); }
  return true;
}

void SceneResources::SetGroundClassification(std::span<const uint32_t> classes,
                                             std::span<const float> palette) {
  const auto classCopy =
      std::make_shared<const std::vector<uint32_t>>(classes.begin(), classes.end());
  const auto paletteCopy =
      std::make_shared<const std::vector<float>>(palette.begin(), palette.end());
  SetGroundClassification({.Classes = {classCopy, classCopy->data()},
                           .ClassWords = classCopy->size(),
                           .Palette = {paletteCopy, paletteCopy->data()},
                           .PaletteFloats = paletteCopy->size()});
}

void SceneResources::SetGroundClassification(GroundClassificationSource source) noexcept {
  GroundClassification_ = std::move(source);
}

size_t SceneResources::HeightPageSourceBytes() const noexcept {
  size_t bytes = 0;
  for (const HeightPage &page : HeightPages_) {
    if (page.Nodes) { bytes += page.Nodes->capacity() * sizeof(float); }
  }
  return bytes;
}

}
