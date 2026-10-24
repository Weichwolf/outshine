#ifndef OUTSHINE_BASE_SCENE_PROJECTEDERRORBUDGET_H
#define OUTSHINE_BASE_SCENE_PROJECTEDERRORBUDGET_H

#include <cmath>

namespace outshine {

struct ProjectedErrorBudget {
  double FocalPx = 0.0;
  double AllowedErrorPx = 1.0;

  [[nodiscard]] constexpr bool Allows(double errorM, double minimumDistanceM) const noexcept {
    if (!std::isfinite(errorM) || errorM < 0.0 || !std::isfinite(FocalPx) || !(FocalPx > 0.0) ||
        !std::isfinite(AllowedErrorPx) || AllowedErrorPx < 0.0 ||
        !std::isfinite(minimumDistanceM) || !(minimumDistanceM > 0.0)) {
      return false;
    }
    if (errorM == 0.0) { return true; }
    if (AllowedErrorPx == 0.0) { return false; }
    const double projected = errorM * FocalPx;
    const double allowed = AllowedErrorPx * minimumDistanceM;
    return std::isfinite(projected) && projected > 0.0 && std::isfinite(allowed) &&
           projected <= allowed;
  }

  [[nodiscard]] constexpr bool operator==(const ProjectedErrorBudget &) const noexcept = default;
};

}

#endif
