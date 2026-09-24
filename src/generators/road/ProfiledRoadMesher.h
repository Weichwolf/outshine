#ifndef OUTSHINE_GENERATORS_ROAD_PROFILEDROADMESHER_H
#define OUTSHINE_GENERATORS_ROAD_PROFILEDROADMESHER_H

#include <span>
#include <cstdint>
#include <vector>

#include "math/Vec3.h"
#include "RoadMesher.h"

namespace outshine::Generators {

class ProfiledRoadMesher final : public RoadMesher {
public:
  [[nodiscard]] RoadMeshingStats
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadMeshBuffers &into) const override;

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &wearsLinear,
                RoadMeshBuffers &into) const override;
};

}
#endif
