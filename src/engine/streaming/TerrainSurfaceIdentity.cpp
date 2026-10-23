#include "TerrainResidency.h"
#include "BlockedDigest.h"
#include "TerrainRenderer.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
std::array<uint8_t, 32> TerrainResidency::SamplesIdentity(std::span<const float> nodes) {
  BlockedDigest digest;
  (void)digest.Number(static_cast<uint64_t>(nodes.size()));
  for (const float value : nodes) { (void)digest.Number(value); }
  return digest.Finish();
}

bool TerrainResidency::AppendSurfacePage(size_t slot,
                                         std::vector<size_t> &slots,
                                         size_t most) const {
  const Held &page = Held_[slot];
  if (!page.Virtual && page.Postings < 2) { return true; }
  if (slots.size() == most) { return false; }
  slots.push_back(slot);
  return true;
}

bool TerrainResidency::SurfaceLevelPages(size_t zoom,
                                         Data::TileId region,
                                         size_t most,
                                         std::vector<size_t> &slots) const {
  const auto level = PageLevels_[zoom];
  if (level.First == level.End) { return true; }
  if (std::cmp_less_equal(zoom, region.Zoom)) {
    const auto shift = static_cast<unsigned>(region.Zoom - static_cast<int>(zoom));
    const Data::TileId parent{
        .Zoom = static_cast<int>(zoom), .X = region.X >> shift, .Y = region.Y >> shift};
    const size_t *slot = PageIndex_.Find(parent);
    return slot == nullptr || AppendSurfacePage(*slot, slots, most);
  }
  const auto shift = static_cast<unsigned>(static_cast<int>(zoom) - region.Zoom);
  const uint32_t lowX = region.X << shift;
  const uint32_t lowY = region.Y << shift;
  const uint32_t side = uint32_t{1} << shift;
  if (uint64_t{side} * side <= level.End - level.First) {
    for (uint32_t x = lowX; x < lowX + side; ++x) {
      for (uint32_t y = lowY; y < lowY + side; ++y) {
        const size_t *slot = PageIndex_.Find({.Zoom = static_cast<int>(zoom), .X = x, .Y = y});
        if (slot != nullptr && !AppendSurfacePage(*slot, slots, most)) { return false; }
      }
    }
    return true;
  }
  for (size_t slot = level.First; slot < level.End; ++slot) {
    const auto tile = Held_[slot].Tile;
    if (tile.X >= lowX && tile.X < lowX + side && tile.Y >= lowY && tile.Y < lowY + side &&
        !AppendSurfacePage(slot, slots, most)) {
      return false;
    }
  }
  return true;
}

std::expected<std::array<uint8_t, 32>, TerrainIdentityError>
TerrainResidency::SurfaceIdentity(Data::TileId region, size_t pagesMost) const {
  if (region.Zoom < 0 || region.Zoom > Data::TileId::MaximumZoom ||
      region.X >= (uint64_t{1} << static_cast<unsigned>(region.Zoom)) ||
      region.Y >= (uint64_t{1} << static_cast<unsigned>(region.Zoom))) {
    return std::unexpected(TerrainIdentityError::InvalidRequest);
  }
  if (!HasPublishedSurface()) { return std::unexpected(TerrainIdentityError::Unpublished); }
  size_t firstZoom = 0;
  for (int zoom = std::min(region.Zoom, static_cast<int>(PageLevels_.size() - 1)); zoom >= 0;
       --zoom) {
    const auto shift = static_cast<unsigned>(region.Zoom - zoom);
    const size_t *slot =
        PageIndex_.Find({.Zoom = zoom, .X = region.X >> shift, .Y = region.Y >> shift});
    if (slot != nullptr && (Held_[*slot].Virtual || Held_[*slot].Postings >= 2)) {
      firstZoom = static_cast<size_t>(zoom);
      break;
    }
  }
  std::vector<size_t> slots;
  slots.reserve(std::min(pagesMost, size_t{16}));
  for (size_t zoom = firstZoom; zoom < PageLevels_.size(); ++zoom) {
    if (!SurfaceLevelPages(zoom, region, pagesMost, slots)) {
      return std::unexpected(TerrainIdentityError::OverBudget);
    }
  }
  if (slots.empty()) { return std::unexpected(TerrainIdentityError::NoSource); }
  BlockedDigest digest;
  constexpr uint32_t samplingVersion = 1;
  (void)digest.Number(samplingVersion);
  (void)digest.Number(Render::TerrainRenderer::kSide);
  for (const size_t slot : slots) {
    const Held &page = Held_[slot];
    (void)digest.Number(page.Tile.Zoom);
    (void)digest.Number(page.Tile.X);
    (void)digest.Number(page.Tile.Y);
    (void)digest.Number(page.Postings);
    (void)digest.Number(static_cast<uint8_t>(page.Virtual));
    (void)digest.Put(page.SamplesDigest);
  }
  (void)digest.Number(static_cast<uint64_t>(slots.size()));
  return digest.Finish();
}
}
