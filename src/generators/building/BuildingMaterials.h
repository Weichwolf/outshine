#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGMATERIALS_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGMATERIALS_H

#include "Vec3.h"
#include "Vec4.h"
#include "Material.h"
#include <array>

namespace outshine::Generators {
inline constexpr Vec3f kBuildingWallColour{{0.74f, 0.71f, 0.65f}};

[[nodiscard]] std::array<Material, 3> BuildingSurfaceMaterials();
[[nodiscard]] Material AverageHouseWallMaterial();
[[nodiscard]] Vec4f BuildingRoofCoating(float pitchedFraction,
                                        const Vec3f &wallColour = kBuildingWallColour);
}
#endif
