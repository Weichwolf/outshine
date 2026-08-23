#include <cstdio>

#include "Check.h"

#include "LightVisibilityStage.h"

using outshine::Render::LightVisibilityStage;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const float sun[3] = {0.5f, -0.7f, 0.1f};
  const float up[3] = {0.0f, 1.0f, 0.0f};
  {
    LightVisibilityStage stage;
    stage.Declare(sun, up, 100.0);
    CHECK(stage.Standing(), "a sound basis stands");
  }
  {
    LightVisibilityStage stage;
    const float noSun[3] = {0.0f, 0.0f, 0.0f};
    stage.Declare(noSun, up, 100.0);
    CHECK(!stage.Standing(),
          "**A ZERO SUN REFUSES AT DECLARE**: the normalisation would divide by zero and "
          "every shadow term would go silently NaN -- a failure is loud (board:1739)");
  }
  {
    LightVisibilityStage stage;
    const float parallel[3] = {0.0f, 1.0f, 0.0f};
    stage.Declare(parallel, up, 100.0);
    CHECK(!stage.Standing(),
          "an up parallel to the sun has no cross product and refuses by the same rule");
  }
  {
    LightVisibilityStage stage;
    stage.Declare(sun, up, 0.0);
    CHECK(!stage.Standing(), "and a radius of nothing frames nothing, as before");
  }

  Covers("V.9 the shadow basis refuses degeneracy at declare: a zero sun, an up parallel "
         "to it, or an empty radius never reach the NaN the normalisation would mint "
         "(board:1739)");
  return Report();
}
