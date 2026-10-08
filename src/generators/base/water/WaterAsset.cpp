#include "WaterAsset.h"
#include "Capacity.h"
#include <utility>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Generators {
WaterAsset::WaterAsset(Data data) : Data_(std::make_shared<const Data>(std::move(data))) {}

const WaterAsset::Data &WaterAsset::Contents() const noexcept {
  static const Data empty;
  return Data_ ? *Data_ : empty;
}

const std::vector<WaterAsset::Surface> &WaterAsset::Surfaces() const noexcept {
  return Contents().Surfaces;
}

const std::vector<WaterAsset::Course> &WaterAsset::Courses() const noexcept {
  return Contents().Courses;
}

const std::vector<float> &WaterAsset::Levels() const noexcept {
  return Contents().Levels;
}

std::span<const double> WaterAsset::Points() const noexcept {
  return Contents().Points;
}

std::span<const WaterAsset::SurfaceRing>
WaterAsset::RingsOf(const Surface &surface) const noexcept {
  return std::span(Contents().Rings).subspan(surface.FirstRing, surface.RingCount);
}

std::span<const WaterAsset::Surface> WaterAsset::OfTile(int tile) const noexcept {
  if (tile < 0 || static_cast<size_t>(tile) >= Contents().Tiles.size()) { return {}; }
  const auto &entry = Contents().Tiles[static_cast<size_t>(tile)];
  return std::span(Contents().Surfaces).subspan(entry.FirstSurface, entry.SurfaceCount);
}

int WaterAsset::TileIndex(int zoom, int x, int y) const noexcept {
  const auto &tiles = Contents().Tiles;
  for (size_t at = 0; at < tiles.size(); ++at) {
    if (tiles[at].Zoom == zoom && tiles[at].X == x && tiles[at].Y == y) {
      return static_cast<int>(at);
    }
  }
  return -1;
}

bool WaterAsset::Complete() const noexcept {
  return Contents().Status == Coverage::Complete;
}

size_t WaterAsset::IngestedTiles() const noexcept {
  return Contents().ProcessedTiles;
}

size_t WaterAsset::HeapBytes() const noexcept {
  const auto &data = Contents();
  return Data_ ? sizeof(Data) + CapacityBytes(data.Surfaces) + CapacityBytes(data.Rings) +
                     CapacityBytes(data.Courses) + CapacityBytes(data.Levels) +
                     CapacityBytes(data.Points) + CapacityBytes(data.Tiles)
               : 0;
}

long WaterAsset::NoGroundCount() const noexcept {
  return Contents().NoGround;
}

long WaterAsset::OutlierCount() const noexcept {
  return Contents().Outliers;
}

long WaterAsset::InvalidBodyCount() const noexcept {
  return Contents().InvalidBodies;
}
}
