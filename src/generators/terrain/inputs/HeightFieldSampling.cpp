#include "HeightField.h"
#include "ChunkSurface.h"
#include "GroundSample.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

namespace outshine::Ground {
namespace {
std::optional<double> Bilinear(const HeightField::Block &block, double fx, double fy) noexcept {
  const auto raster = block.Raster;
  const double x = fx * static_cast<double>(raster.Postings - 1);
  const double y = fy * static_cast<double>(raster.Postings - 1);
  const int column = ChunkNodeCell(x, raster);
  const int row = ChunkNodeCell(y, raster);
  const auto c0 = ChunkNodePosting(column, raster.Postings, raster.Side);
  const auto c1 = ChunkNodePosting(column + 1, raster.Postings, raster.Side);
  const auto r0 = ChunkNodePosting(row, raster.Postings, raster.Side);
  const auto r1 = ChunkNodePosting(row + 1, raster.Postings, raster.Side);
  const double u = (x - c0) / static_cast<double>(c1 - c0);
  const double v = (y - r0) / static_cast<double>(r1 - r0);
  const auto nodes = block.Terrain ? block.Terrain->Heights() : std::span(block.Nodes);
  const auto at =
      static_cast<size_t>(row) * static_cast<size_t>(raster.Side) + static_cast<size_t>(column);
  const auto stride = static_cast<size_t>(raster.Side);
  const std::array<double, 4> values{
      nodes[at], nodes[at + 1], nodes[at + stride], nodes[at + stride + 1]};
  if (!std::ranges::all_of(values, GroundSample::HeightIsOnEarth)) { return std::nullopt; }
  const double height =
      (1 - v) * ((1 - u) * values[0] + u * values[1]) + v * ((1 - u) * values[2] + u * values[3]);
  return GroundSample::At(height).AslM();
}
}

std::optional<HeightField::LocatedSample>
HeightField::SampleBilinearAt(LongitudeLatitude at) const noexcept {
  if (Fallback_ || Zoom_ < 0 || Zoom_ > MaximumTileZoom || !std::isfinite(at.LongitudeDeg) ||
      !std::isfinite(at.LatitudeDeg) || std::abs(at.LatitudeDeg) > kMercatorLatMaxDeg) {
    return std::nullopt;
  }
  const auto frac = ToTileFracClamped(
      {.LongitudeDeg = Wrap180(at.LongitudeDeg), .LatitudeDeg = at.LatitudeDeg}, Zoom_);
  const double count = std::ldexp(1., Zoom_);
  const double x = frac.X >= count ? 0. : frac.X;
  const double y = std::clamp(frac.Y, 0., count);
  const auto xTile = static_cast<long>(std::floor(x));
  const auto yTile = static_cast<long>(std::min(std::floor(y), count - 1));
  std::optional<size_t> selected;
  for (size_t index = 0; index < Blocks_.size(); ++index) {
    const auto &block = Blocks_[index];
    if (block.At.Zoom != Zoom_ || block.At.X != xTile || block.At.Y != yTile) { continue; }
    if (selected) { return std::nullopt; }
    selected = index;
  }
  if (!selected) { return std::nullopt; }
  const auto &block = Blocks_[*selected];
  if (!HasRaster(block) || block.MissingBoundary || block.Sources.empty()) { return std::nullopt; }
  const auto height =
      Bilinear(block, x - static_cast<double>(xTile), y - static_cast<double>(yTile));
  return height ? std::optional(
                      LocatedSample{.Height = GroundSample::At(*height), .BlockIndex = *selected})
                : std::nullopt;
}
}
