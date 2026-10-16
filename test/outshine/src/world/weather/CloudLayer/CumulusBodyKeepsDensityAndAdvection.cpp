#include "CloudLayer.h"
#include "Check.h"

#include <array>
#include <cmath>

namespace {
constexpr double kBaseM = 2572.0;
constexpr double kPhaseS = 37.0;
constexpr double kReferenceTolerance = 1e-14;
constexpr double kAdvectionTolerance = 1e-10;
constexpr double kFlatRadiusM = 1e14;
constexpr double kAdvanceS = 120.0;
constexpr double kMaximumDensityScale = 1.5;

struct Sample {
  outshine::Vec3 Position;
  double Extinction;
};

constexpr std::array kSamples{
    Sample{.Position = {{0.0, 2800.0, 0.0}}, .Extinction = 0.0},
    Sample{.Position = {{1000.0, 2900.0, -3000.0}}, .Extinction = 0.001187398446657182},
    Sample{.Position = {{-14000.0, 2900.0, 14000.0}}, .Extinction = 0.0},
    Sample{.Position = {{5000.0, 3000.0, -7000.0}}, .Extinction = 0.00009745723251757415},
    Sample{.Position = {{-3000.0, 2700.0, 1000.0}}, .Extinction = 0.0},
    Sample{.Position = {{200.0, 2650.0, 400.0}}, .Extinction = 0.000062594753445807945},
    Sample{.Position = {{10000.0, 2800.0, 10000.0}}, .Extinction = 0.0},
    Sample{.Position = {{-1200.0, 2800.0, -1200.0}}, .Extinction = 0.0088978614101539871}};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CloudLayer layer{.BaseM = kBaseM,
                   .Cover = 0.65,
                   .WindMs = CloudWindFrom({.FromDeg = 240.0, .SpeedMs = 8.0}),
                   .Profile = CloudProfile::CumulusV1};
  for (const auto &sample : kSamples) {
    const double actual = CloudExtinction(layer, sample.Position, kPhaseS);
    CHECK(std::abs(actual - sample.Extinction) < kReferenceTolerance,
          "versioned cumulus density matches independent FP64 values including negative cells");
    double previous = 0.0;
    for (const double cover : {0.0, 0.25, 0.45, 0.65, 1.0}) {
      auto changed = layer;
      changed.Cover = cover;
      const double value = CloudExtinction(changed, sample.Position, kPhaseS);
      CHECK(std::isfinite(value) && value >= previous &&
                value <= kMaximumDensityScale * layer.ExtinctionPerM,
            "coverage is monotone and density remains finite nonnegative and bounded");
      previous = value;
    }
  }
  for (const double height : {layer.BaseM, layer.BaseM + layer.ThicknessM}) {
    CHECK(CloudExtinction(layer, {{0.0, height, 0.0}}, kPhaseS) == 0.0,
          "cumulus remains inside the common physical shell");
  }
  layer.RadiusM = kFlatRadiusM;
  for (const auto &sample : kSamples) {
    const auto shifted = sample.Position + layer.WindMs * kAdvanceS;
    CHECK(std::abs(CloudExtinction(layer, shifted, kPhaseS + kAdvanceS) -
                   CloudExtinction(layer, sample.Position, kPhaseS)) < kAdvectionTolerance,
          "body placement and erosion advect together on the nearly flat limit");
  }
  return Report();
}
