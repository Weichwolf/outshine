#include <span>
#include <vector>
#include <cstdint>
#include "TerrainRenderer.h"
#include <array>

namespace outshine::Render {
void TerrainRenderer::SyncInputs() {
  CulledGeneration_.reset();
  VisibleReal_ = Resident_->RealCount_;
  VisibleVirtual_ = Resident_->VirtualCount_;
  Selection_.BeginFrame();
}

Native::Result<void> TerrainRenderer::SetGrid(std::span<const float> fractions) {
  auto changed = Resident_->SetGrid(fractions);
  if (changed) { SyncInputs(); }
  return changed;
}

Native::Result<PageId> TerrainRenderer::PlacePage(std::span<const float> nodes) {
  auto changed = Resident_->PlacePage(nodes);
  if (changed) { SyncInputs(); }
  return changed;
}

Native::Result<std::vector<PageId>>
TerrainRenderer::PlacePages(std::span<const HeightPageView> nodes) {
  auto changed = Resident_->PlacePages(nodes);
  if (changed) { SyncInputs(); }
  return changed;
}

void TerrainRenderer::ReleasePage(PageId which) {
  Resident_->ReleasePage(which);
  SyncInputs();
}

Native::Result<void> TerrainRenderer::SetInstances(std::span<const GroundTile> real,
                                                   std::span<const GroundTile> virtual_) {
  auto changed = Resident_->SetInstances(real, virtual_);
  if (changed) { SyncInputs(); }
  return changed;
}

uint64_t TerrainRenderer::HeldBytes() const {
  uint64_t bytes = Resident_->HeldBytes() + Selection_.HeldBytes();
  for (const auto *buffer : std::array<const Native::Buffer *, 3>{
           Visible_.get(), VisibleStaging_.get(), PatchList_.get()}) {
    if (buffer != nullptr) { bytes += buffer->Description().Bytes; }
  }
  return bytes;
}
}
