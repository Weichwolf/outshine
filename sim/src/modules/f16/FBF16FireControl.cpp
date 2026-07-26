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
  if (!state.Nav.H.Readable() || !state.Platform.H.Readable()) {
    state.FireControl.H.Invalidate();
    return;
  }
  float horizFt = state.Nav.SteerDistNm * 6076.12f;
  float altDiffFt = state.Platform.AltM * kMToFt - state.Nav.SteerElevFt;
  state.FireControl.SteerSlantNm = std::sqrt(horizFt * horizFt + altDiffFt * altDiffFt) * kFtToNm;
  state.FireControl.RangeProvider = 'B';
  state.FireControl.H.Publish(state.NowS);
}

} // namespace FlightBox
