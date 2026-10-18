#include "src/client/PlaceTurn.h"
#include "Check.h"
#include <cmath>
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double fullTurnDeg = 360.0;
  constexpr double tolerance = 1e-12;
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
  for (std::size_t at = 0; at < views.size(); ++at) {
    const auto &view = views[(at + 1) % views.size()];
    const double expected =
        at == 59 ? initial.Geographic.BearingDeg
                 : std::remainder(185.0 + 6.0 * static_cast<double>(at + 1), fullTurnDeg);
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
  for (const double rate : {25.0, 30.0, 60.0}) {
    const auto timing = Client::FrameTiming::At(rate);
    CHECK(timing.has_value(), "standard capture profiles resolve");
    const auto turn = Shots::PlaceTurn(initial, origin, *timing);
    CHECK(turn.size() == static_cast<std::size_t>(rate),
          "one second uses exactly the requested number of camera samples");
    for (std::size_t at = 1; at < turn.size(); ++at) {
      CHECK(std::abs(std::remainder(turn[at].Geographic.BearingDeg - initial.Geographic.BearingDeg -
                                        fullTurnDeg * static_cast<double>(at) / rate,
                                    fullTurnDeg)) < tolerance,
            "each profile covers the full circle uniformly");
    }
    CHECK(turn.front().Id == initial.Id &&
              turn.front().Geographic.BearingDeg == initial.Geographic.BearingDeg,
          "the last measured frame selects the unchanged initial camera at every rate");
  }
  initial.Geographic.Geodetic = {};
  initial.Geographic.SamplesHeight = true;
  initial.OffsetM = {{2.0, 3.0, 4.0}};
  initial.Sees.LooksAt = true;
  initial.Sees.LookAtM = {{2.0, 13.0, -6.0}};
  initial.Sees.UpM = {{0.0, 1.0, 0.0}};
  const auto targeted = Shots::PlaceTurn(initial, {});
  CHECK(std::abs(targeted[15].Sees.LookAtM[0] - 12.0) < tolerance &&
            std::abs(targeted[15].Sees.LookAtM[1] - 13.0) < tolerance &&
            std::abs(targeted[15].Sees.LookAtM[2] - 4.0) < tolerance,
        "a clockwise quarter turn moves a northern explicit target east around the offset eye");
  CHECK(targeted[15].Geographic.SamplesHeight && targeted[15].Sees.UpM[1] == 1.0,
        "terrain height translates along the rotation axis and preserves the turn");
  return Report();
}
