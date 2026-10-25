#ifndef OUTSHINE_GENERATORS_ROAD_ROADCROSSSECTION_H
#define OUTSHINE_GENERATORS_ROAD_ROADCROSSSECTION_H

#include "RoadMesher.h"

namespace outshine::Generators {

[[nodiscard]] Section RoadSection(double halfWidthM, RoadProfile profile);
[[nodiscard]] RoadGate
RoadEndGate(std::span<const RoadStation> stations, bool atEnd, Section section);
[[nodiscard]] std::array<Vec3f, kRibbonAcross> RoadPortPositions(const RoadPort &port);
[[nodiscard]] Vec3f RoadPortNormal(RoadPlane plane);

}
#endif
