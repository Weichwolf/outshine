#ifndef OUTSHINE_SCENE_PROJECTEDERRORBUDGET_H
#define OUTSHINE_SCENE_PROJECTEDERRORBUDGET_H

#include <cmath>

namespace outshine {

/// Pinhole displacement-estimate policy shared by content producers and detail selection.
/// One pixel is the default policy, not a surface certificate or an invisibility guarantee.
/// Focal length and distance alone do not bound off-axis perspective or occlusion effects.
struct ProjectedErrorBudget {
  double FocalPx = 0.0; ///< Finite positive focal length in output pixels; zero is unspecified.
  double AllowedErrorPx = 1.0; ///< Finite nonnegative allowance; zero requires zero displacement.

  /// Test errorM * FocalPx <= AllowedErrorPx * minimumDistanceM without accepting nonfinite terms.
  /// @param errorM Known finite nonnegative displacement bound, in metres; unknown is rejected.
  /// @param minimumDistanceM Finite positive distance estimate in metres for the candidate bounds.
  /// Invalid inputs and positive errors that underflow the estimate are rejected conservatively.
  /// The caller owns both estimates; acceptance does not prove their correctness or visibility.
  /// @return True for validated zero displacement or a finite estimate within the allowance.
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

  /// Compare every quality input; changing the allowance invalidates dependent detail products.
  /// @return True if the focal length and pixel allowance match exactly.
  [[nodiscard]] constexpr bool operator==(const ProjectedErrorBudget &) const noexcept = default;
};

}

#endif
