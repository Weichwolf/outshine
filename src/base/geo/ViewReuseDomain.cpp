#include "ViewReuseDomain.h"
#include "Units.h"
#include "Wgs84.h"
#include <cmath>
#include <optional>

namespace outshine {
bool ViewReuseDomain::Contains(LongitudeLatitude eye,
                               ProjectedErrorBudget projection,
                               std::optional<Vec3> eyeEcef) const noexcept {
  if (Projection != projection || !std::isfinite(Projection.FocalPx) || Projection.FocalPx <= 0.0 ||
      !std::isfinite(Projection.AllowedErrorPx) || Projection.AllowedErrorPx < 0.0 ||
      !std::isfinite(EyeRadiusM) || EyeRadiusM < 0.0 || !std::isfinite(Eye.LongitudeDeg) ||
      !std::isfinite(Eye.LatitudeDeg) || EyeEcef.has_value() != eyeEcef.has_value()) {
    return false;
  }
  if (EyeEcef) {
    const Vec3 offset = *EyeEcef - *eyeEcef;
    const double distanceM = std::hypot(offset[0], offset[1], offset[2]);
    return std::isfinite(distanceM) && distanceM <= EyeRadiusM;
  }
  const Ellipsoid earth{.SemiMajorM = kWgs84A, .Flattening = 1.0 - std::sqrt(1.0 - kWgs84E2)};
  const auto distance =
      GeodesicOn({.LongitudeDeg = Eye.LongitudeDeg, .LatitudeDeg = Eye.LatitudeDeg},
                 {.LongitudeDeg = eye.LongitudeDeg, .LatitudeDeg = eye.LatitudeDeg},
                 earth);
  return distance.Converged && std::isfinite(distance.AlongM) && distance.AlongM <= EyeRadiusM;
}

}
