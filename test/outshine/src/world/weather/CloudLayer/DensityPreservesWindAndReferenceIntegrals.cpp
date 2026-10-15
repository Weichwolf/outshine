#include "CloudLayer.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace {
constexpr double kWindSpeedMs = 8.0;
constexpr double kPhaseS = 37.0;
constexpr double kFieldTolerance = 1e-14;
constexpr double kOpticalTolerance = 1e-11;
constexpr double kHorizonTolerance = 1e-10;
constexpr double kWindTolerance = 1e-12;
constexpr double kVerticalOptical = 5.0237203411816278;
constexpr double kHorizonOptical = 114.4712790560923;
constexpr double kReceiverBeforeM = 700.0;
constexpr double kDegreesPerTurn = 360.0;

struct Sample {
  outshine::Vec3 Position;
  double Extinction;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const CloudLayer layer{.BaseM = 2572.0,
                         .Cover = 0.65,
                         .WindMs = CloudWindFrom({.FromDeg = 240.0, .SpeedMs = kWindSpeedMs})};
  // Same fixed declared inputs, independently evaluated by cloud_field.py in float64.
  constexpr std::array samples{
      Sample{.Position = {{0.0, 2800.0, 0.0}}, .Extinction = 0.012006089018296740},
      Sample{.Position = {{1000.0, 2900.0, -3000.0}}, .Extinction = 0.0054912025173033518},
      Sample{.Position = {{-14000.0, 2900.0, 14000.0}}, .Extinction = 0.0042744735298376642},
      Sample{.Position = {{5000.0, 3000.0, -7000.0}}, .Extinction = 0.0072382116205554773}};
  for (const Sample &sample : samples) {
    CHECK(std::abs(CloudExtinction(layer, sample.Position, kPhaseS) - sample.Extinction) <
              kFieldTolerance,
          "native world-anchored extinction agrees with the independent Python field");
  }
  const Vec3 origin{{0.0, 1772.0, 0.0}};
  CHECK(std::abs(CloudOpticalDepth(layer, origin, {{0.0, 1.0, 0.0}}, {.Seconds = kPhaseS}) -
                 kVerticalOptical) < kOpticalTolerance,
        "vertical optical depth agrees with the Python midpoint reference");
  CHECK(std::abs(CloudOpticalDepth(layer, origin, {{1.0, 0.0, 0.0}}, {.Seconds = kPhaseS}) -
                 kHorizonOptical) < kHorizonTolerance,
        "the curved horizon uses the same bounded density integration");
  CHECK(CloudOpticalDepth(
            layer, origin, {{0.0, 1.0, 0.0}}, {.Seconds = kPhaseS, .MaximumM = kReceiverBeforeM}) ==
            0.0,
        "a surface before the layer excludes every cloud sample behind it");
  const double partial =
      CloudOpticalDepth(layer, origin, {{0.0, 1.0, 0.0}}, {.Seconds = kPhaseS, .MaximumM = 1100.0});
  CHECK(partial > 0.0 &&
            partial < CloudOpticalDepth(layer, origin, {{0.0, 1.0, 0.0}}, {.Seconds = kPhaseS}),
        "a surface inside the layer clips the optical depth at its own distance");
  const std::array bearings{0.0, 90.0, 180.0, 270.0};
  constexpr std::array<Vec3, 4> expected{
      {{{0.0, 0.0, 8.0}}, {{-8.0, 0.0, 0.0}}, {{0.0, 0.0, -8.0}}, {{8.0, 0.0, 0.0}}}};
  for (size_t index = 0; index < bearings.size(); ++index) {
    const Vec3 wind = CloudWindFrom({.FromDeg = bearings[index], .SpeedMs = kWindSpeedMs});
    CHECK(Length(wind - expected[index]) < kWindTolerance,
          "meteorological wind-from maps to east/up/south with the correct sign");
    CHECK(Length(wind - CloudWindFrom({.FromDeg = bearings[index] + (2.0 * kDegreesPerTurn),
                                       .SpeedMs = kWindSpeedMs})) < kWindTolerance,
          "stored unwrapped bearings describe the same wind after full turns");
  }
  CHECK(CloudExtinction(layer, {{0.0, layer.BaseM, 0.0}}, kPhaseS) == 0.0 &&
            CloudExtinction(layer, {{0.0, layer.BaseM + layer.ThicknessM, 0.0}}, kPhaseS) == 0.0,
        "density vanishes at both physical layer boundaries");
  const Vec3 point{{1000.0, 2900.0, -3000.0}};
  CHECK(CloudExtinction(layer, point, (kPhaseS + 1.0)) != CloudExtinction(layer, point, kPhaseS),
        "advancing the shared wind phase changes the field without rebuilding geometry");
  CloudLayer clear = layer;
  clear.Cover = 0.0;
  CHECK(CloudOpticalDepth(clear, origin, {{0.0, 1.0, 0.0}}, {.Seconds = kPhaseS}) == 0.0,
        "zero declared cloud cover is exactly transparent");
  return Report();
}
