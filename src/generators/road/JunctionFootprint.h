#ifndef OUTSHINE_GENERATORS_ROAD_JUNCTIONFOOTPRINT_H
#define OUTSHINE_GENERATORS_ROAD_JUNCTIONFOOTPRINT_H

#include "RoadMesher.h"
#include "Earth.h"

#include <span>
#include <vector>

namespace outshine::Generators {

inline constexpr double kJunctionCoreReachM = 4.0;

struct JunctionFootprint {
  EastNorth Centre;
  std::vector<EastNorth> Rim;
};

[[nodiscard]] JunctionFootprint BuildJunctionFootprint(std::span<const RoadGate> gates);

}
#endif
