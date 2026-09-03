#ifndef OUTSHINE_GENERATORS_ROAD_ROADMESH_H
#define OUTSHINE_GENERATORS_ROAD_ROADMESH_H

#include <span>
#include <cstdint>
#include <vector>

#include "math/Vec3.h"
#include "RoadMesher.h"

namespace outshine::Generators {

class RoadMesh final : public RoadMesher {
public:
  void Design(std::span<RoadStation> along, double mostGradient, double leastCrestK) const override;

  [[nodiscard]] RoadTallied
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadRaised &into) const override;

  void Junction(std::span<const RoadGate> gates,
                const Vec3f &wearsLinear,
                RoadRaised &into) const override;
};

} // namespace outshine::Generators
#endif
