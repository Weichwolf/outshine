#ifndef OUTSHINE_GENERATORS_ROAD_ROADPAGERECIPE_H
#define OUTSHINE_GENERATORS_ROAD_ROADPAGERECIPE_H

#include "RoadPageGeometry.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct SurfaceBody;

struct RoadPageInterval {
  uint32_t Edge = 0;
  uint32_t Lane = 0;
  std::vector<RoadStation> Stations;
  RoadSweep Surface;
  std::shared_ptr<const SurfaceBody> StepBody = nullptr;
};

struct RoadPageJunction {
  uint32_t Index = 0;
  uint64_t Node = 0;
  std::vector<RoadGate> Gates;
  RoadPlane Plane;
  Vec3f WearsLinear;
};

struct RoadPageRecipe {
  std::vector<RoadPageInterval> Intervals;
  std::vector<RoadPageJunction> Junctions;
};

[[nodiscard]] std::expected<RoadPageGeometry, std::string_view> MeshRoadPage(
    const RoadMesher &mesher, const RoadPageRecipe &recipe, const std::stop_token &stop = {});
}
#endif
