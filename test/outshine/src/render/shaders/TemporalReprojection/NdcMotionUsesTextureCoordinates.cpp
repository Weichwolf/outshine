#include "Check.h"
#include <array>

namespace {
struct vec2 {
  float x;
  float y;
};

#include "src/render/shaders/temporalReprojection.glsl"
}

int main() {
  using namespace outshine::Test;

  struct Case {
    vec2 NowUv;
    vec2 MotionNdc;
    vec2 PreviousUv;
  };

  const std::array<Case, 5> cases{{
      {{0.65f, 0.35f}, {0.3f, 0.3f}, {0.5f, 0.5f}},
      {{0.25f, 0.75f}, {-0.5f, -0.5f}, {0.5f, 0.5f}},
      {{0.5f, 0.5f}, {0.0f, 0.0f}, {0.5f, 0.5f}},
      {{0.1f, 0.9f}, {0.4f, 0.4f}, {-0.1f, 1.1f}},
      {{0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}},
  }};
  for (const auto &test : cases) {
    const vec2 previous = temporalHistoryUv(test.NowUv, test.MotionNdc, {0.0f, 0.0f});
    CHECK_NEAR(previous.x, test.PreviousUv.x, 1e-6, "previous U", "NDC width is twice UV width");
    CHECK_NEAR(previous.y, test.PreviousUv.y, 1e-6, "previous V", "NDC and texture Y oppose");
  }
  const vec2 texelShift{2.0f / 1280.0f, -2.0f / 720.0f};
  const vec2 previous =
      temporalHistoryUv({0.5f, 0.5f}, texelShift, {1.0f / 1280.0f, 1.0f / 720.0f});
  CHECK_NEAR(previous.x, 0.5, 1e-6, "jitter U", "stationary history stays on the pixel grid");
  CHECK_NEAR(previous.y, 0.5, 1e-6, "jitter V", "only sample jitter changes in a stationary frame");
  return Report();
}
