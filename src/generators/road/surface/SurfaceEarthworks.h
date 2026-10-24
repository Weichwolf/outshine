#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEEARTHWORKS_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEEARTHWORKS_H

#include "SurfaceBody.h"
#include "RoadTerrainContact.h"
#include "GroundMesher.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
enum class SurfaceGroundEffect : uint8_t {
  Unspecified,
  GroundContact,
  OverheadClearance,
  NoEarthwork
};

struct SurfaceGroundSource {
  SurfaceGroundEffect Effect = SurfaceGroundEffect::Unspecified;
  uint64_t ContactKey = 0;
  double ClearanceM = 0;
  double ReliefM = 0;
};

struct SurfaceEarthworkBudget {
  size_t Faces = HeightSolidBudget{}.SurfaceFaces;
  double ApronM = RoadTerrainContact::VergeM;
};

struct SurfaceEarthworks {
  std::vector<EarthworkStamp> Stamps;
  std::vector<uint32_t> FaceParents;
  size_t NoEarthworkFaces = 0;
};

enum class SurfaceEarthworkError : uint8_t {
  InvalidInput,
  ConflictingContactKeys,
  CapacityExceeded,
  Canceled
};

[[nodiscard]] std::expected<SurfaceEarthworks, SurfaceEarthworkError>
BuildSurfaceEarthworks(const SurfaceBody &body,
                       std::span<const SurfaceGroundSource> sources,
                       SurfaceEarthworkBudget budget = {},
                       const std::stop_token &stop = {});
}
#endif
