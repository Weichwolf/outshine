#ifndef OUTSHINE_BASE_GEO_VIEWREUSEDOMAIN_H
#define OUTSHINE_BASE_GEO_VIEWREUSEDOMAIN_H
#include "Geodesy.h"
#include "ProjectedErrorBudget.h"
#include "Vec3.h"
#include <optional>

namespace outshine {
struct ViewReuseDomain {
  LongitudeLatitude Eye;
  std::optional<Vec3> EyeEcef;
  ProjectedErrorBudget Projection;
  double EyeRadiusM = 0;
  [[nodiscard]] bool Contains(LongitudeLatitude eye,
                              ProjectedErrorBudget projection,
                              std::optional<Vec3> eyeEcef = std::nullopt) const noexcept;
};
}
#endif
