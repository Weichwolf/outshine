#include "SceneRenderer.h"
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SceneRenderer renderer;
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Failed,
        "an unconfigured scene has no linear frame to read");
  CHECK(!renderer.FilterSceneRadiance({.WidthPx = 16, .HeightPx = 16}),
        "radiance filtering refuses a missing submitted source without dereferencing a plan");
  return Report();
}
