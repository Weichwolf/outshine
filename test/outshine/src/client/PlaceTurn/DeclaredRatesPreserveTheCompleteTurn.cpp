#include "src/client/PlaceTurn.h"
#include "Check.h"
#include <array>
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
  constexpr std::array<double, 3> rates{25.0, 30.0, 60.0};
  for (const double rate : rates) {
    const auto profile = Shots::FrameProfile::ForPlace(rate);
    CHECK(profile.has_value(), "declared measurement profile is valid");
    if (!profile) { continue; }
    const auto views = Shots::PlaceTurn(initial, origin, *profile);
    CHECK(views.size() == static_cast<std::size_t>(rate),
          "a one-second turn has the declared number of render views");
    CHECK(profile->BudgetMs() == 1000.0 / rate,
          "each profile keeps its own independent frame budget");
    for (std::size_t index = 0; index < views.size(); ++index) {
      const auto &view = views[(index + 1) % views.size()];
      const double angle = 360.0 * static_cast<double>(index + 1) / rate;
      const double expected = std::remainder(initial.Geographic.BearingDeg + angle, 360.0);
      CHECK(std::abs(std::remainder(view.Geographic.BearingDeg - expected, 360.0)) < 1e-12,
            "every measured view advances its full-circle share");
      CHECK(view.Geographic.Geodetic == initial.Geographic.Geodetic &&
                view.Geographic.PitchDeg == initial.Geographic.PitchDeg &&
                view.Sees.FovDeg == initial.Sees.FovDeg,
            "rate changes preserve station, pitch and lens");
    }
    CHECK(views.front().Id == initial.Id &&
              views.front().Geographic.BearingDeg == initial.Geographic.BearingDeg,
          "the last measured selection reuses the exact original pose");
  }
  initial.Geographic.Geodetic = {};
  initial.Geographic.SamplesHeight = true;
  initial.OffsetM = {{2.0, 3.0, 4.0}};
  initial.Sees.LooksAt = true;
  initial.Sees.LookAtM = {{2.0, 13.0, -6.0}};
  initial.Sees.UpM = {{0.0, 1.0, 0.0}};
  for (const double rate : rates) {
    const auto profile = Shots::FrameProfile::ForPlace(rate);
    if (!profile) { continue; }
    const auto views = Shots::PlaceTurn(initial, {}, *profile);
    for (std::size_t index = 0; index < views.size(); ++index) {
      const double angle = 2.0 * kPi * static_cast<double>(index) / rate;
      CHECK(std::abs(views[index].Sees.LookAtM[0] - (2.0 + 10.0 * std::sin(angle))) < 1e-12 &&
                std::abs(views[index].Sees.LookAtM[1] - 13.0) < 1e-12 &&
                std::abs(views[index].Sees.LookAtM[2] - (4.0 - 10.0 * std::cos(angle))) < 1e-12,
            "explicit targets turn around the same fixed eye at each declared rate");
      CHECK(views[index].Geographic.SamplesHeight &&
                std::abs(views[index].Sees.UpM[1] - 1.0) < 1e-12,
            "height sampling and the vertical axis survive the rate change");
    }
  }
  return Report();
}
