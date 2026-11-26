#ifndef OUTSHINE_WORLD_PRODUCTS_SURFACETILE_H
#define OUTSHINE_WORLD_PRODUCTS_SURFACETILE_H

#include "TileId.h"
#include "Earth.h"
#include "Vec3.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct SurfaceVertex {
  Vec3f PositionM;
  Vec3f Normal;
};

struct SurfaceTile {
  static constexpr unsigned Version = 1;
  static constexpr size_t MaximumVertices = 65536;
  static constexpr size_t MaximumIndices = 6 * MaximumVertices;
  static constexpr size_t MaximumPackedBytes =
      164 + 6 * sizeof(float) * MaximumVertices + sizeof(uint32_t) * MaximumIndices;

  Data::TileId Coverage;
  LongitudeLatitude Origin;
  std::string Revision;
  size_t StoredBytes = 0;
  Vec3 MinimumM;
  Vec3 MaximumM;
  double GeometricErrorM = 0;
  std::vector<SurfaceVertex> Vertices;
  std::vector<uint32_t> Indices;
};

[[nodiscard]] std::expected<SurfaceTile, std::string> ReadSurfaceTile(std::string_view path);
[[nodiscard]] std::expected<SurfaceTile, std::string>
DecodePackedSurfaceTile(std::span<const uint8_t> bytes);

}
#endif
