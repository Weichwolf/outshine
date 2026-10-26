#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_ROADPAGEMESHING_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_ROADPAGEMESHING_H

#include "HeightSolid.h"
#include "RoadPageGeometry.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Generators {
struct RoadPageIntervalView {
  uint32_t Edge = 0;
  uint64_t Source = 0;
  std::span<const RoadStation> Stations;
  RoadSweep Surface;
  const HeightSolid *Body = nullptr;
};

struct RoadPageJunctionView {
  uint32_t Index = 0;
  uint64_t Node = 0;
  std::span<const RoadGate> Gates;
  RoadPlane Plane;
  Vec3f WearsLinear;
};

[[nodiscard]] std::expected<void, std::string_view> AppendRoadPageInterval(
    const RoadMesher &mesher, const RoadPageIntervalView &interval, RoadPageGeometry &product);
void AppendRoadPageJunction(const RoadMesher &mesher,
                            const RoadPageJunctionView &junction,
                            RoadPageGeometry &product);
}
#endif
