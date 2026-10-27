#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEEARTHWORKS_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEEARTHWORKS_H

#include "SurfaceBody.h"
#include "HeightSolidTerrainContact.h"
#include "RoadTerrainContact.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct SurfaceEarthworkBudget {
  size_t Faces = HeightSolidBudget{}.SurfaceFaces;
  double ApronM = RoadTerrainContact::VergeM;
};

struct SurfaceEarthworks {
  std::vector<EarthworkStamp> Stamps;
  std::vector<uint32_t> FaceParents;
  size_t NoEarthworkFaces = 0;
};

[[nodiscard]] std::expected<SurfaceEarthworks, SurfaceEarthworkError>
BuildSurfaceEarthworks(const SurfaceBody &body,
                       std::span<const SurfaceGroundSource> sources,
                       SurfaceEarthworkBudget budget = {},
                       const std::stop_token &stop = {});
}
#endif
