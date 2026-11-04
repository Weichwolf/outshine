#include <span>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
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

Native::Result<std::shared_ptr<ClipmapSurface>> TerrainRenderer::CreateClipmaps(ClipmapGrid grid,
                                                                                size_t bytesMost) {
  if (Resident_->Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "clipmaps require a terrain device"});
  }
  auto surface = ClipmapSurface::Make(*Resident_->Device_, grid, bytesMost);
  if (!surface) { return std::unexpected(surface.error()); }
  const auto ready =
      Pipelines_->ConfigureClipmaps(*Resident_->Device_,
                                    grid.HalfCells,
                                    bytesMost - static_cast<size_t>((*surface)->HeldBytes()));
  if (!ready) { return std::unexpected(ready.error()); }
  Clipmaps_ = *surface;
  ClipmapGeometryBytesMost_ = bytesMost - static_cast<size_t>((*surface)->HeldBytes());
  return surface;
}

uint64_t TerrainRenderer::HeldBytes() const {
  uint64_t bytes = Resident_->HeldBytes() + Selection_.HeldBytes();
  if (Clipmaps_) { bytes += Clipmaps_->DeviceBytes() + Pipelines_->Clipmaps().HeldBytes(); }
  for (const auto *buffer : std::array<const Native::Buffer *, 3>{
           Visible_.get(), VisibleStaging_.get(), PatchList_.get()}) {
    if (buffer != nullptr) { bytes += buffer->Description().Bytes; }
  }
  return bytes;
}
}
