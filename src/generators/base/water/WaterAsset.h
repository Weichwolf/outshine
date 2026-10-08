#ifndef OUTSHINE_GENERATORS_WATER_WATERASSET_H
#define OUTSHINE_GENERATORS_WATER_WATERASSET_H

#include "Earth.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
class WaterAsset {
public:
  struct Surface {
    uint32_t FirstRing = 0, RingCount = 0;
    float LevelM = 0.0f;
  };

  struct SurfaceRing {
    uint32_t FirstPoint = 0, PointCount = 0;
  };

  struct Course {
    uint32_t FirstPoint = 0, PointCount = 0;
    uint32_t FirstLevel = 0;
    float HalfWidthM = 0.0f;
  };

  struct Tile {
    int32_t Zoom = 0, X = 0, Y = 0;
    uint32_t FirstSurface = 0, SurfaceCount = 0;
  };
  enum class Coverage { Partial, Complete };

  struct Data {
    std::vector<Surface> Surfaces;
    std::vector<SurfaceRing> Rings;
    std::vector<Course> Courses;
    std::vector<float> Levels;
    std::vector<double> Points;
    std::vector<Tile> Tiles;
    uint32_t ProcessedTiles = 0;
    long NoGround = 0, Outliers = 0, InvalidBodies = 0;
    Coverage Status = Coverage::Complete;
  };

  WaterAsset() = default;
  explicit WaterAsset(Data data);
  [[nodiscard]] const std::vector<Surface> &Surfaces() const noexcept;
  [[nodiscard]] const std::vector<Course> &Courses() const noexcept;
  [[nodiscard]] const std::vector<float> &Levels() const noexcept;
  [[nodiscard]] std::span<const double> Points() const noexcept;
  [[nodiscard]] std::span<const SurfaceRing> RingsOf(const Surface &surface) const noexcept;
  [[nodiscard]] std::span<const Surface> OfTile(int tile) const noexcept;
  [[nodiscard]] int TileIndex(int zoom, int x, int y) const noexcept;
  [[nodiscard]] std::optional<float> LevelAt(LongitudeLatitude at) const noexcept;
  [[nodiscard]] bool Complete() const noexcept;
  [[nodiscard]] size_t IngestedTiles() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;
  [[nodiscard]] long NoGroundCount() const noexcept;
  [[nodiscard]] long OutlierCount() const noexcept;
  [[nodiscard]] long InvalidBodyCount() const noexcept;
  [[nodiscard]] std::expected<std::vector<uint8_t>, std::string>
  EncodeNative(size_t bytesMost) const;
  [[nodiscard]] static std::expected<WaterAsset, std::string>
  DecodeNative(std::span<const uint8_t> bytes);

private:
  [[nodiscard]] const Data &Contents() const noexcept;
  std::shared_ptr<const Data> Data_;
};
}
#endif
