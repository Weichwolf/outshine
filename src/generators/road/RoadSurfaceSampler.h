#ifndef OUTSHINE_GENERATORS_ROAD_ROADSURFACESAMPLER_H
#define OUTSHINE_GENERATORS_ROAD_ROADSURFACESAMPLER_H

#include <optional>

#include "RoadSurfaceBuilder.h"
#include "math/Vec3.h"

namespace outshine::Generators {

struct RoadSurfaceContact {
  Vec3 PositionM;
  Vec3 Normal;
  World::TransportEdgeId SourceEdge;
  double StationM = 0.0;
  double LateralOffsetM = 0.0;
};

class RoadSurfaceSampler {
public:
  [[nodiscard]] static std::optional<RoadSurfaceContact> At(const RoadAlignment &alignment,
                                                            const RoadSurface &surface,
                                                            double stationM,
                                                            double lateralOffsetM);
};

}

#endif
