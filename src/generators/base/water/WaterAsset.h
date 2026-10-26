#ifndef OUTSHINE_GENERATORS_BASE_WATER_WATERASSET_H
#define OUTSHINE_GENERATORS_BASE_WATER_WATERASSET_H

#include "Earth.h"
#include "TileSourceIdentity.h"
#include "WaterSurfaceMesh.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
class WaterAsset {
public:
  static constexpr uint32_t NoInput = std::numeric_limits<uint32_t>::max();

  struct Surface {
    uint32_t FirstRing = 0, RingCount = 0;
    float LevelM = 0.0f;
    uint32_t Input = NoInput;
  };

  struct SurfaceRing {
    uint32_t FirstPoint = 0, PointCount = 0;
  };

  struct Course {
    uint32_t FirstPoint = 0, PointCount = 0;
    uint32_t FirstLevel = 0;
    float HalfWidthM = 0.0f;
    uint32_t Input = NoInput;
  };

  struct Tile {
    int32_t Zoom = 0, X = 0, Y = 0;
    uint32_t FirstSurface = 0, SurfaceCount = 0;
  };
  enum class Coverage { Partial, Complete };

  struct Source {
    outshine::Data::TileSourceIdentity Identity;
    std::string InputDigest;
    uint32_t FeatureCount = 0;
  };

  struct Input {
    uint32_t Source = 0, Feature = 0;
    std::optional<uint64_t> FeatureId;
    std::string Layer, Kind, Crossing;
    bool Bridge = false, Tunnel = false;
    std::optional<bool> Intermittent;
  };

  enum class SurfaceRejectionReason { InvalidTopology, CapacityExceeded, InvalidHeightModel };

  struct SurfaceRejection {
    uint32_t Surface = 0;
    SurfaceRejectionReason Reason = SurfaceRejectionReason::InvalidTopology;
  };

  struct SurfaceGeometry {
    WaterSurfaceMesh Mesh;
    std::vector<SurfaceRejection> Rejections;
    std::vector<uint32_t> FaceSourceOffsets;
    std::vector<uint32_t> FaceSources;
    std::vector<SurfaceRejection> ProfileDeclines;
  };

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
    std::vector<Source> Sources;
    std::vector<Input> Inputs;
  };

  WaterAsset() = default;
  explicit WaterAsset(Data data);
  [[nodiscard]] const std::vector<Surface> &Surfaces() const noexcept;
  [[nodiscard]] const std::vector<Course> &Courses() const noexcept;
  [[nodiscard]] const std::vector<float> &Levels() const noexcept;
  [[nodiscard]] std::span<const double> Points() const noexcept;
  [[nodiscard]] std::span<const Source> Sources() const noexcept;
  [[nodiscard]] std::span<const Input> Inputs() const noexcept;
  [[nodiscard]] const Input *InputOf(const Surface &surface) const noexcept;
  [[nodiscard]] const Input *InputOf(const Course &course) const noexcept;
  [[nodiscard]] std::span<const SurfaceRing> RingsOf(const Surface &surface) const noexcept;
  [[nodiscard]] std::span<const Surface> OfTile(int tile) const noexcept;
  [[nodiscard]] int TileIndex(int zoom, int x, int y) const noexcept;
  [[nodiscard]] std::optional<double> LevelAt(LongitudeLatitude at) const noexcept;
  [[nodiscard]] const SurfaceGeometry *PreparedSurfaces() const noexcept;
  [[nodiscard]] bool SurfacesReady() const noexcept;

  [[nodiscard]] bool SharesInputsWith(const WaterAsset &other) const noexcept {
    return Data_ == other.Data_;
  }

  [[nodiscard]] std::expected<WaterAsset, std::string> WithSurfaces(SurfaceGeometry geometry) const;
  [[nodiscard]] bool Complete() const noexcept;
  [[nodiscard]] size_t IngestedTiles() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;
  [[nodiscard]] size_t PreparedSurfaceBytes() const noexcept;
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
  std::shared_ptr<const SurfaceGeometry> Geometry_;
};
}
#endif
