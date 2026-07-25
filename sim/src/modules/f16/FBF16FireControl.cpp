#include "FBF16FireControl.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr float kMToFt = 3.280839895f;
constexpr float kFtToNm = 1.0f / 6076.12f;
} // namespace

/* Slant = sqrt(horizontal^2 + altDiff^2), the 'B' (baro/steerpoint-elevation) method. */
void FBF16FireControl::Run(FBState &state, double dt) {
  (void)dt;
  float horizFt = state.steerDistNm * 6076.12f;
  float altDiffFt = state.alt * kMToFt - state.steerElevFt;
  state.steerSlantNm = std::sqrt(horizFt * horizFt + altDiffFt * altDiffFt) * kFtToNm;
  state.rangeProvider = 'B';
}

} // namespace FlightBox
