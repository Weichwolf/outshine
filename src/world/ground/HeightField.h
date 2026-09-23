#ifndef OUTSHINE_WORLD_GROUND_HEIGHTFIELD_H
#define OUTSHINE_WORLD_GROUND_HEIGHTFIELD_H

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "geo/Geodesy.h"
#include "TerrainGrid.h"
#include "TerrainLoader.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

class HeightField {
public:
  struct Block {
    TileSpot At;
    Sampling Raster;
    std::shared_ptr<const TerrainField> Terrain;
    std::vector<float> Nodes;
    std::vector<Data::TileSourceIdentity> Sources;
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
    into.Terrain.reset();
    into.Nodes.assign(block.Nodes(), block.Nodes() + side * side);
    into.Sources.assign(block.Sources().begin(), block.Sources().end());
    return true;
  }

  [[nodiscard]] static bool CopiesField(const TerrainGrid &grid, Data::TileId at, Block &into) {
    const TerrainField *field = grid.TryField();
    return field != nullptr && CopiesField(*field, at, into);
  }

  [[nodiscard]] static bool CopiesField(const TerrainField &field, Data::TileId at, Block &into) {
    if (field.Rows() != field.Cols() || field.Cols() < 2) { return false; }
    into.At = {.Zoom = at.Zoom, .X = static_cast<long>(at.X), .Y = static_cast<long>(at.Y)};
    into.Raster = {.Side = static_cast<int>(field.Cols()), .Postings = field.Cols()};
    into.Terrain.reset();
    into.Nodes.assign(field.Data(),
                      field.Data() + static_cast<size_t>(field.Rows()) * field.Cols());
    into.Sources.assign(field.Sources().begin(), field.Sources().end());
    return true;
  }

  [[nodiscard]] static bool
  SharesField(std::shared_ptr<const TerrainField> field, Data::TileId at, Block &into) {
    if (!field || field->Rows() != field->Cols() || field->Cols() < 2) { return false; }
    into.At = {.Zoom = at.Zoom, .X = static_cast<long>(at.X), .Y = static_cast<long>(at.Y)};
    into.Raster = {.Side = static_cast<int>(field->Cols()), .Postings = field->Cols()};
    into.Nodes.clear();
    into.Sources.assign(field->Sources().begin(), field->Sources().end());
    into.Terrain = std::move(field);
    return true;
  }

  template <typename Sample>
  [[nodiscard]] static bool SamplesField(Data::TileId at, int side, Sample &&sample, Block &into) {
    if (side < 2) { return false; }
    Block sampled;
    sampled.At = {.Zoom = at.Zoom, .X = static_cast<long>(at.X), .Y = static_cast<long>(at.Y)};
    sampled.Raster = {.Side = side, .Postings = static_cast<uint32_t>(side)};
    sampled.Nodes.resize(static_cast<size_t>(side) * static_cast<size_t>(side));
    const auto denominator = static_cast<double>(side - 1);
    for (int row = 0; row < side; ++row) {
      for (int column = 0; column < side; ++column) {
        const Geo point = TileFracToGeo(
            {.X = static_cast<double>(at.X) + static_cast<double>(column) / denominator,
             .Y = static_cast<double>(at.Y) + static_cast<double>(row) / denominator},
            at.Zoom);
        const std::optional<double> height = sample(LongitudeLatitude{
            .LongitudeDeg = point.LongitudeDeg, .LatitudeDeg = point.LatitudeDeg});
        if (!height) { return false; }
        sampled.Nodes[static_cast<size_t>(row) * static_cast<size_t>(side) +
                      static_cast<size_t>(column)] = static_cast<float>(*height);
      }
    }
    into = std::move(sampled);
    return true;
  }

  [[nodiscard]] static std::shared_ptr<const HeightField>
  Of(int zoom, std::vector<Block> blocks, bool fallback = false) {
    return std::shared_ptr<const HeightField>(new HeightField(zoom, std::move(blocks), fallback));
  }

  [[nodiscard]] bool Fallback() const noexcept { return Fallback_; }

  [[nodiscard]] bool Qualified() const noexcept { return Qualified_; }

  [[nodiscard]] std::span<const Data::TileSourceIdentity> Sources() const noexcept {
    return Sources_;
  }

  [[nodiscard]] std::span<const Block> Blocks() const noexcept { return Blocks_; }

  [[nodiscard]] GroundSample At(LongitudeLatitude at) const noexcept {
    const TileSpot spot = SpotOf(at, Zoom_);
    for (const Block &one : Blocks_) {
      if (one.At.X != spot.X || one.At.Y != spot.Y) { continue; }
      double held = 0.0;
      const float *const nodes = one.Terrain ? one.Terrain->Data() : one.Nodes.data();
      GroundBlock::Over(nodes, one.At, one.Raster).AslMRow(at, 0.0, std::span<double>(&held, 1));
      return GroundSample::At(held);
    }
    return GroundSample::Missing();
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = Sources_.capacity() * sizeof(Data::TileSourceIdentity);
    for (const auto &source : Sources_) {
      bytes += source.SourceId.capacity() + source.Revision.capacity();
    }
    for (const Block &one : Blocks_) {
      bytes += (one.Terrain ? one.Terrain->HeapBytes() : 0u) +
               one.Nodes.capacity() * sizeof(float) +
               one.Sources.capacity() * sizeof(Data::TileSourceIdentity);
      for (const auto &source : one.Sources) {
        bytes += source.SourceId.capacity() + source.Revision.capacity();
      }
    }
    return bytes;
  }

private:
  HeightField(int zoom, std::vector<Block> blocks, bool fallback)
      : Blocks_(std::move(blocks)), Zoom_(zoom), Fallback_(fallback) {
    Qualified_ = !fallback;
    for (const Block &block : Blocks_) {
      Qualified_ = Qualified_ && !block.Sources.empty();
      Sources_.insert(Sources_.end(), block.Sources.begin(), block.Sources.end());
    }
    std::ranges::sort(Sources_);
    Sources_.erase(std::ranges::unique(Sources_).begin(), Sources_.end());
  }

  std::vector<Block> Blocks_;
  std::vector<Data::TileSourceIdentity> Sources_;
  int Zoom_ = 0;
  bool Fallback_ = false;
  bool Qualified_ = false;
};

}
#endif
