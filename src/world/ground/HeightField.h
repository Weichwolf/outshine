#ifndef OUTSHINE_WORLD_GROUND_HEIGHTFIELD_H
#define OUTSHINE_WORLD_GROUND_HEIGHTFIELD_H

#include <memory>
#include <span>
#include <vector>

#include "geo/Geodesy.h"
#include "TerrainLoader.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

class HeightField {
public:
  struct Block {
    TileSpot At;
    Sampling Raster;
    std::vector<float> Nodes;
  };

  [[nodiscard]] static TileSpot SpotOf(LongitudeLatitude at, int zoom) noexcept {
    const TileFrac f = ToTileFracClamped(
        Geo{.LongitudeDeg = Wrap180(at.LongitudeDeg), .LatitudeDeg = at.LatitudeDeg}, zoom);
    long x = static_cast<long>(f.X);
    const long y = static_cast<long>(f.Y);
    (void)WrapTile(zoom, &x, &y);
    return {.Zoom = zoom, .X = x, .Y = y};
  }

  [[nodiscard]] static bool Copies(const GroundBlock &block, Block &into) {
    if (block.Where() != GroundBlock::State::Resolved || block.Nodes() == nullptr) { return false; }
    const Sampling raster = block.Raster();
    const auto side = static_cast<size_t>(raster.Side);
    if (side < 2) { return false; }
    into.At = block.Spot();
    into.Raster = raster;
    into.Nodes.assign(block.Nodes(), block.Nodes() + side * side);
    return true;
  }

  [[nodiscard]] static std::shared_ptr<const HeightField> Of(int zoom, std::vector<Block> blocks) {
    return std::shared_ptr<const HeightField>(new HeightField(zoom, std::move(blocks)));
  }

  [[nodiscard]] GroundSample At(LongitudeLatitude at) const noexcept {
    const TileSpot spot = SpotOf(at, Zoom_);
    for (const Block &one : Blocks_) {
      if (one.At.X != spot.X || one.At.Y != spot.Y) { continue; }
      double held = 0.0;
      GroundBlock::Over(one.Nodes.data(), one.At, one.Raster)
          .AslMRow(at, 0.0, std::span<double>(&held, 1));
      return GroundSample::At(held);
    }
    return GroundSample::Missing();
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = 0;
    for (const Block &one : Blocks_) { bytes += one.Nodes.capacity() * sizeof(float); }
    return bytes;
  }

private:
  HeightField(int zoom, std::vector<Block> blocks) : Blocks_(std::move(blocks)), Zoom_(zoom) {}

  std::vector<Block> Blocks_;
  int Zoom_ = 0;
};

} // namespace outshine::Ground
#endif
