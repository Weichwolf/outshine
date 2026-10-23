#ifndef OUTSHINE_BASE_SPATIAL_PLANARSTATIONMESH_H
#define OUTSHINE_BASE_SPATIAL_PLANARSTATIONMESH_H

#include "PlanarMesh.h"
#include <cstddef>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine {
struct PlanarStationAxis {
  PlanarPoint Origin{};
  PlanarPoint Direction{};
  std::vector<double> Cuts;
};

struct PlanarStationBudget {
  size_t Vertices = 4096;
  size_t Faces = 8192;
  size_t Axes = 16;
  size_t Cuts = 32;
  size_t FaceVisits = 262144;
};

struct PlanarStationWork {
  size_t FaceVisits = 0;
  size_t EdgeIntersections = 0;
};

struct PlanarStationMesh {
  PlanarMesh Mesh;
  std::vector<std::vector<double>> Stations;
  PlanarStationWork Work;
};

[[nodiscard]] std::expected<PlanarStationMesh, PlanarMeshError>
CutPlanarStations(std::span<const PlanarPoint> points,
                  std::span<const PlanarFace> faces,
                  std::span<const PlanarStationAxis> axes,
                  PlanarStationBudget budget = {},
                  const std::stop_token &stop = {});
}
#endif
