#include "src/client/PlaceTurn.h"
#include "Check.h"
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::View initial;
  initial.Id = "station";
  initial.Placement = Scenario::CameraPlacement::Geodetic;
  initial.Geographic.Geodetic = {.LongitudeDeg = 12.0, .LatitudeDeg = 48.0, .HeightM = 492.0};
  initial.Geographic.BearingDeg = 185.0;
  initial.Geographic.PitchDeg = -1.6;
  initial.Sees.FovDeg = 30.68;
  const LongitudeLatitude origin{.LongitudeDeg = 12.0, .LatitudeDeg = 48.0};
  const auto views = Shots::PlaceTurn(initial, origin);
  CHECK(views.size() == 60, "a complete turn contains sixty fixed camera views");
  for (size_t at = 0; at < views.size(); ++at) {
    const auto &view = views[(at + 1) % views.size()];
    const double expected = at == 59
                                ? initial.Geographic.BearingDeg
                                : std::remainder(185.0 + 6.0 * static_cast<double>(at + 1), 360.0);
    CHECK(view.Geographic.BearingDeg == expected, "each measured frame advances six degrees");
    CHECK(view.Geographic.Geodetic.HeightM == 492.0 &&
              view.Geographic.Geodetic.LatitudeDeg == 48.0 &&
              view.Geographic.Geodetic.LongitudeDeg == 12.0 && view.Geographic.PitchDeg == -1.6 &&
              view.Sees.FovDeg == 30.68,
          "turn preserves station, pitch and lens");
  }
  CHECK(views.front().Id == initial.Id &&
            views.front().Geographic.BearingDeg == initial.Geographic.BearingDeg,
        "final selection reuses the exact original view without trigonometric drift");
  initial.Geographic.Geodetic = {};
  initial.Geographic.SamplesHeight = true;
  initial.OffsetM = {{2.0, 3.0, 4.0}};
  initial.Sees.LooksAt = true;
  initial.Sees.LookAtM = {{2.0, 13.0, -6.0}};
  initial.Sees.UpM = {{0.0, 1.0, 0.0}};
  const auto targeted = Shots::PlaceTurn(initial, {});
  CHECK(std::abs(targeted[15].Sees.LookAtM[0] - 12.0) < 1e-12 &&
            std::abs(targeted[15].Sees.LookAtM[1] - 13.0) < 1e-12 &&
            std::abs(targeted[15].Sees.LookAtM[2] - 4.0) < 1e-12,
        "a clockwise quarter turn moves a northern explicit target east around the offset eye");
  CHECK(targeted[15].Geographic.SamplesHeight && targeted[15].Sees.UpM[1] == 1.0,
        "terrain height translates along the rotation axis and preserves the turn");
  return Report();
}
