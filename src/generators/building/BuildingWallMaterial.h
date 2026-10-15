#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGWALLMATERIAL_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGWALLMATERIAL_H

#include "BuildingShape.h"
#include "math/Vec2.h"
#include <cmath>

namespace outshine::Generators {

inline constexpr int kMasonryWallVariant = 6;

[[nodiscard]] inline bool UsesWallMaterialMetres(const BuildingShape &shape) noexcept {
  return shape.WallVariant == kMasonryWallVariant && shape.OpeningStyle != FacadeStyle::Glazing;
}

struct BuildingWallMaterial {
  EastNorth Origin;
  EastNorth Along;
  double BaseM = 0.0;

  [[nodiscard]] static BuildingWallMaterial
  Of(const BuildingShape &shape, const EastNorth &from, const EastNorth &to) noexcept {
    const double e = to.EastM - from.EastM;
    const double n = to.NorthM - from.NorthM;
    const double length = std::hypot(e, n);
    const double inverse = length > 0.0 ? 1.0 / length : 0.0;
    return {.Origin = from,
            .Along = {.EastM = e * inverse, .NorthM = n * inverse},
            .BaseM = shape.SeatM + shape.FootM};
  }

  [[nodiscard]] Vec2f At(const EastNorth &point, double heightM) const noexcept {
    return {{static_cast<float>((point.EastM - Origin.EastM) * Along.EastM +
                                (point.NorthM - Origin.NorthM) * Along.NorthM),
             static_cast<float>(heightM - BaseM)}};
  }
};

}
#endif
