#include "SurfaceMaterial.h"
#include "Check.h"

#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  constexpr Vec4f kSubstrate{{.2f, .1f, .05f, .9f}};
  constexpr Vec4f kLitter{{.05f, .2f, .3f, .6f}};
  constexpr int kSourceClass = 7;
  constexpr float kLitterCoverage = .75f;
  constexpr double kColourTolerance = 1e-7;
  SurfaceMaterial row{};
  row.Ground = kSubstrate;
  row.Litter = kLitter;
  row.GroundClass = kSourceClass;
  CHECK(row.MeanGround() == row.Ground, "zero litter retains exact substrate appearance");
  row.Mix[0] = 1;
  CHECK(row.MeanGround() == row.Litter, "full litter retains exact covering appearance");
  row.Mix[0] = kLitterCoverage;
  constexpr Vec4f expected{{.0875f, .175f, .2375f, .675f}};
  const auto mixed = row.MeanGround();
  for (size_t channel = 0; channel < 4; ++channel) {
    CHECK_NEAR(mixed[channel],
               expected[channel],
               kColourTolerance,
               "declared coverage",
               "native mean layer");
  }
  CHECK(row.GroundClass == kSourceClass && row.Ground[3] == kSubstrate[3] &&
            row.Litter[3] == kLitter[3],
        "appearance composition preserves substrate identity and source finishes");
  return Report();
}
