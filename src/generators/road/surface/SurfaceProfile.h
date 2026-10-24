#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEPROFILE_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEPROFILE_H

#include "PlanarMesh.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
using SurfaceProfileTriangle = std::array<std::array<double, 3>, 3>;

struct SurfaceProfilePlane {
  PlanarPoint Center{};
  PlanarPoint Tangent{};
  double HalfSpanM = 0;
};

struct SurfaceProfileBudget {
  static constexpr size_t kDefaultTriangles = 128;
  size_t Triangles = kDefaultTriangles;
  size_t Points = 2 * kDefaultTriangles;
};

struct SurfaceProfileWork {
  size_t TriangleVisits = 0;
  size_t EdgeTests = 0;
  size_t ScratchHeapBytes = 0;
  size_t OutputHeapBytes = 0;
};

struct SurfaceProfile {
  std::vector<std::array<double, 3>> Points;
  std::vector<uint32_t> Faces;
  size_t AxisVertex = 0;
  SurfaceProfileWork Work;
};

enum class SurfaceProfileError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  MissingSupport,
  ClippedProfile,
  DisconnectedProfile,
  OverlappingProfile
};

[[nodiscard]] std::expected<SurfaceProfile, SurfaceProfileError>
CutSurfaceProfile(std::span<const SurfaceProfileTriangle> triangles,
                  SurfaceProfilePlane plane,
                  SurfaceProfileBudget budget = {},
                  const std::stop_token &stop = {});
}
#endif
