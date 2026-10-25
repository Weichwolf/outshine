#ifndef OUTSHINE_GENERATORS_ROAD_ROADLANECOLOUR_H
#define OUTSHINE_GENERATORS_ROAD_ROADLANECOLOUR_H

#include "Vec3.h"

namespace outshine::Ground {
class VegetationTemplates;
}

namespace outshine::Generators {
[[nodiscard]] Vec3f RoadLaneColour(int coverRow, const Ground::VegetationTemplates &vegetation);
}
#endif
