#include "Check.h"
#include "Engine.h"
#include "TestEngine.h"
#include <SDL3/SDL.h>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    Engine engine;
    const auto target = engine.setRenderTarget(Extent{.WidthPx = 32, .HeightPx = 32});
    CHECK(target, "an offscreen target opens");
    if (!target) { return Report(); }
    Scenario::Document scenario;
    scenario.Render.Declared = true;
    scenario.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
    scenario.Render.Outputs = {"surface"};
    const auto declared = engine.declare(scenario);
    CHECK(declared, declared ? "the initial world is published" : declared.error().c_str());
    if (!declared) { return Report(); }
    auto &renderer = TestEngine::StateOf(engine).Picture.Device;
    CHECK(renderer.DeviceUsable(), "the published renderer is ready");
    std::string error;
    const bool began = renderer.BeginsWorldCandidate(error);
    CHECK(began, "a subsequent candidate opens");
    if (began) {
      CHECK(renderer.DeviceUsable(), "an unedited candidate retains published state selection");
      {
        const auto editing = renderer.EditWorldCandidate();
        CHECK(!renderer.DeviceUsable(), "an explicit edit selects the uninitialized candidate");
      }
      CHECK(renderer.DeviceUsable(), "ending the edit restores published state selection");
      renderer.AbandonsWorldCandidate();
      CHECK(renderer.DeviceUsable(), "abandonment retains the published renderer");
    }
  }
  SDL_Quit();
  return Report();
}
