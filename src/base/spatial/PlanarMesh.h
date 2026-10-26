#ifndef OUTSHINE_BASE_SPATIAL_PLANARMESH_H
#define OUTSHINE_BASE_SPATIAL_PLANARMESH_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine {
using PlanarPoint = std::array<double, 2>;
using PlanarFace = std::array<uint32_t, 3>;

enum class PlanarMeshError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  WeldDistanceExceeded
};

struct PlanarMeshBudget {
  size_t Vertices = 4096;
  size_t Faces = 8192;
  size_t Candidates = 65536;
};

struct PlanarMeshWork {
  size_t WeldCandidates = 0;
  size_t EdgeCandidates = 0;
  size_t SplitEdges = 0;
  size_t RemovedFaces = 0;
  size_t Passes = 0;
  double MaximumWeldDistance = 0.0;
  double EdgeTolerance = 0.0;
};

struct PlanarMesh {
  std::vector<PlanarPoint> Points;
  std::vector<PlanarFace> Faces;
  std::vector<uint32_t> PointRemap;
  std::vector<uint32_t> FaceParents;
  PlanarMeshWork Work;
};

[[nodiscard]] std::expected<PlanarMesh, PlanarMeshError>
ConformPlanarMesh(std::span<const PlanarPoint> points,
                  std::span<const PlanarFace> faces,
                  double weldDistance,
                  PlanarMeshBudget budget = {},
                  const std::stop_token &stop = {});
}
#endif
