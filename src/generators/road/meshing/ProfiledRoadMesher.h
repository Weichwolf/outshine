#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_PROFILEDROADMESHER_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_PROFILEDROADMESHER_H

#include <span>

#include "Vec3.h"
#include "RoadMesher.h"
#include "RoadDisplaySampler.h"
#include <optional>

namespace outshine::Generators {

class ProfiledRoadMesher final : public RoadMesher {
public:
  ProfiledRoadMesher() = default;

  // Fitted Upper position model only; sampling budgets span a Sweep, apart from fitting/corners.
  explicit ProfiledRoadMesher(RoadDisplayRequest display) : Display_(display) {}

  [[nodiscard]] RoadMeshingStats
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadMeshBuffers &into) const override;

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &wearsLinear,
                RoadMeshBuffers &into) const override;

private:
  std::optional<RoadDisplayRequest> Display_;
};

}
#endif
