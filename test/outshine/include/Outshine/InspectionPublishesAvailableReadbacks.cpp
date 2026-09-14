#include <Outshine.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video initializes");
  if (!initialized) { return Report(); }
  {
    Engine engine;
    CHECK(engine.drawsInto(Extent{32, 32}).has_value(), "offscreen target configured");
    Scenario::Document document;
    document.Render.Declared = true;
    document.Render.Frame = {32, 32};
    document.Render.Outputs = {"surface", "sceneLinear"};
    Scenario::View view;
    view.Id = "inspection-camera";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.PositionM = {{0, 0, 2}};
    view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
    document.Views.push_back(view);
    const bool ready = engine.declare(document) && engine.assemble() && engine.advance();
    CHECK(ready, "explicit scene camera is ready");
    if (ready) {
      std::vector<uint8_t> pixels;
      CHECK(engine.renderer().readPixels(pixels).has_value() && pixels.size() == 32u * 32u * 4u,
            "public readback provides complete RGBA image");
      CHECK(engine.inspect().has_value(), "public inspection reads the available frame");
      int peak = 0;
      for (size_t at = 0; at + 3 < pixels.size(); at += 4) {
        for (size_t channel = 0; channel < 3; ++channel) {
          peak = std::max(peak, static_cast<int>(pixels[at + channel]));
        }
      }
      const auto find = [&](std::string_view name) -> const Measure * {
        for (const auto &measure : engine.measures()) {
          if (measure.What == name) { return &measure; }
        }
        return nullptr;
      };
      const auto *presented = find("the brightest the presented frame shows");
      CHECK(presented && presented->How == peak && presented->Unit == "of 255",
            "inspection publishes RGB peak with byte units, excluding alpha");
      const auto *exposure = find("the exposure the picture applied");
      CHECK(exposure && std::isfinite(exposure->How) && exposure->How > 0,
            "inspection publishes a finite positive applied exposure");
      CHECK(engine.inspect().has_value(), "unchanged frame can be inspected repeatedly");
    }
  }
  SDL_Quit();
  return Report();
}
