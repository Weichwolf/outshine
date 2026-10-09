#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGWALLNORMALS_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGWALLNORMALS_H

#include "BuildingShape.h"
#include <cstddef>

namespace outshine::Generators {
[[nodiscard]] bool HasCurvedShaftWalls(const BuildingShape &shape) noexcept;
[[nodiscard]] Vec3 BuildingWallShadingNormal(const BuildingShape &shape,
                                             const EastNorth &point,
                                             const Vec3 &geometricNormal) noexcept;
[[nodiscard]] Vec3 BuildingWallInterpolatedNormal(const BuildingShape &shape,
                                                  size_t edge,
                                                  const EastNorth &point,
                                                  const Vec3 &geometricNormal) noexcept;
}
#endif
