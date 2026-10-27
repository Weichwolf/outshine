#ifndef OUTSHINE_GENERATORS_ROAD_ROADLANECOLOUR_H
#define OUTSHINE_GENERATORS_ROAD_ROADLANECOLOUR_H

#include "Vec3.h"

namespace outshine::Generators {
class RegionSurfaceRecipes;
[[nodiscard]] Vec3f RoadLaneColour(int coverRow, const RegionSurfaceRecipes &vegetation);
}
#endif
