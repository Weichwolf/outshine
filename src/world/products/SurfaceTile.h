#ifndef OUTSHINE_WORLD_PRODUCTS_SURFACETILE_H
#define OUTSHINE_WORLD_PRODUCTS_SURFACETILE_H

#include "TileId.h"
#include "Earth.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct SurfaceVertex {
  Vec3f PositionM;
  Vec3f Normal;
};

using SurfaceColour = std::array<uint8_t, 4>;

struct SurfaceTile {
  static constexpr unsigned Version = 1;
  static constexpr size_t MaximumVertices = 65536;
  static constexpr size_t MaximumIndices = 6 * MaximumVertices;
  static constexpr size_t MaximumPackedBytes =
      172 + (6 * sizeof(float) + sizeof(SurfaceColour)) * MaximumVertices +
      sizeof(uint32_t) * MaximumIndices;

  Data::TileId Coverage;
  LongitudeLatitude Origin;
  std::string Revision;
  size_t StoredBytes = 0;
  Vec3 MinimumM;
  Vec3 MaximumM;
  double GeometricErrorM = 0;
  uint32_t GridSide = 0;
  uint32_t GroundIndices = 0;
  bool GroundRoughness = false;
  std::vector<SurfaceVertex> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<SurfaceColour> Colours;
};

[[nodiscard]] std::expected<SurfaceTile, std::string> ReadSurfaceTile(std::string_view path);
[[nodiscard]] std::expected<SurfaceTile, std::string>
DecodePackedSurfaceTile(std::span<const uint8_t> bytes);

struct SurfaceCoverageInterval {
  uint64_t Begin;
  uint64_t End;
  [[nodiscard]] bool operator==(const SurfaceCoverageInterval &) const = default;
};

[[nodiscard]] std::optional<std::vector<SurfaceCoverageInterval>>
SurfaceCoverage(std::span<const SurfaceTile> tiles);

}
#endif
