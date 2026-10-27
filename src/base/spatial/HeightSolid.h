#ifndef OUTSHINE_BASE_SPATIAL_HEIGHTSOLID_H
#define OUTSHINE_BASE_SPATIAL_HEIGHTSOLID_H

#include "PlanarMesh.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine {
struct HeightSolidInput {
  std::span<const PlanarPoint> Points;
  std::span<const PlanarFace> Faces;
  std::span<const std::array<double, 3>> TopHeights;
  std::span<const double> BottomHeights;
};

struct HeightSolidBudget {
  size_t Points = 4096;
  size_t SurfaceFaces = 8192;
  size_t Vertices = 32768;
  size_t Faces = 65536;
  size_t EdgeVisits = 262144;
};

struct HeightSolidWork {
  size_t NodeQueries = 0;
  size_t IndexedHeights = 0;
  size_t HeightComparisons = 0;
  size_t IntermediateHeights = 0;
  size_t WallQueries = 0;
  size_t EdgeVisits = 0;
  size_t IndexCapacityBytes = 0;
};

enum class HeightSolidWallKind : uint8_t { Boundary, Discontinuity, Interface };

struct HeightSolid {
  std::vector<std::array<double, 3>> Points;
  std::vector<PlanarFace> Faces;
  std::vector<uint32_t> WallParents;
  std::vector<HeightSolidWallKind> WallKinds;
  size_t SurfaceFaces = 0;
  double VolumeM3 = 0;
  HeightSolidWork Work;
};

[[nodiscard]] bool HeightSolidExposesFace(const HeightSolid &body, size_t face) noexcept;

[[nodiscard]] std::optional<std::array<uint32_t, 2>>
HeightSolidWallSurfaceEdge(const HeightSolid &body, size_t wall) noexcept;

enum class HeightSolidError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  NonManifold,
  CrossingHeights,
  Unrepresentable
};

[[nodiscard]] std::expected<HeightSolid, HeightSolidError> BuildHeightSolid(
    const HeightSolidInput &input, HeightSolidBudget budget = {}, const std::stop_token &stop = {});
}
#endif
