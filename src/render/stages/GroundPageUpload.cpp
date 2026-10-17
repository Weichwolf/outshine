#include "GroundPageUpload.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>

namespace outshine::Render {
Native::Result<void> UploadHeightPages(Native::Device &device,
                                       Native::Commands &commands,
                                       Native::Texture &atlas,
                                       std::span<const PageId> pages,
                                       std::span<const HeightPageView> nodes) {
  constexpr uint64_t pageBytes = GroundLattice::kPageNodes * sizeof(float);
  auto staging = device.CreateBuffer({.Bytes = pageBytes * nodes.size(),
                                      .Use = Native::BufferUse::CopySource,
                                      .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = device.MapWrite(**staging, {.Bytes = pageBytes * nodes.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  for (size_t at = 0; at < nodes.size(); ++at) {
    std::memcpy(mapped->data() + at * pageBytes, nodes[at].data(), pageBytes);
  }
  const auto unmapped = device.Unmap(**staging);
  if (!unmapped) { return unmapped; }
  for (size_t at = 0; at < pages.size(); ++at) {
    const PageId page = pages[at];
    const auto copied = commands.Copy(
        **staging,
        atlas,
        {.BufferOffset = at * pageBytes,
         .RowBytes = static_cast<uint64_t>(GroundLattice::kPageSide) * sizeof(float),
         .Region = {.Layer = page / GroundLattice::kPagesPerLayer,
                    .X = (page % GroundLattice::kPageColumns) *
                         static_cast<uint32_t>(GroundLattice::kPageSide),
                    .Y = ((page % GroundLattice::kPagesPerLayer) / GroundLattice::kPageColumns) *
                         static_cast<uint32_t>(GroundLattice::kPageSide),
                    .Width = static_cast<uint32_t>(GroundLattice::kPageSide),
                    .Height = static_cast<uint32_t>(GroundLattice::kPageSide)}});
    if (!copied) { return copied; }
  }
  return {};
}
}
