#include "ScenarioStartup.h"
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video starts for UI composition");
  for (const double step : {16.0, std::numeric_limits<double>::max(), 0.0}) {
    Engine engine;
    CHECK(engine.setRoots({.BuiltinDataDirectory = "data",
                           .SourceCacheDirectory = {},
                           .AssetCacheDirectory = {}}),
          "data roots are accepted");
    CHECK(engine.setRenderTarget(Extent{128, 128}).has_value(), "offscreen target is configured");
    Scenario::Document scene;
    scene.WheelStepPx = step;
    scene.Surfaces.push_back(
        {.Document = R"(<div id="scroll"><div id="content"></div></div>)",
         .Style = "#scroll { width: 64px; height: 64px; overflow: scroll; }"
                  "#content { width: 32px; height: 128px; }",
         .Programme = {},
         .Where = {.LeftFrac = 0.5, .TopFrac = 0.5, .WidthFrac = 0.5, .HeightFrac = 0.5}});
    const auto declared = engine.loadScenario(scene);
    CHECK(declared.has_value(), declared ? "scroll surface declared" : declared.error().c_str());
    if (declared) {
      SDL_Event event{};
      event.type = SDL_EVENT_MOUSE_WHEEL;
      event.wheel.mouse_x = event.wheel.mouse_y = 80;
      const auto scroll = [&](float amount, bool expected) {
        event.wheel.y = amount;
        const auto result = engine.handleEvent(event);
        CHECK(result && *result == expected,
              "scroll result matches movement at the event position");
      };
      if (step == 0.0) {
        scroll(-1, false);
        continue;
      }
      if (step == std::numeric_limits<double>::max()) {
        event.wheel.y = -2;
        CHECK(!engine.handleEvent(event), "overflow in pixel displacement rejected");
        continue;
      }
      for (const double invalidStep : {-1.0,
                                       std::numeric_limits<double>::quiet_NaN(),
                                       std::numeric_limits<double>::infinity(),
                                       -std::numeric_limits<double>::infinity()}) {
        auto invalid = scene;
        invalid.WheelStepPx = invalidStep;
        CHECK(!LoadFreshScenario(invalid), "startup rejects an invalid wheel step");
      }
      for (float *field : {&event.wheel.mouse_x, &event.wheel.mouse_y, &event.wheel.y}) {
        const float saved = *field;
        for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity()}) {
          *field = invalid;
          CHECK(!engine.handleEvent(event), "nonfinite wheel input is a processing error");
        }
        *field = saved;
      }
      scroll(0, false);
      scroll(1, false);
      scroll(-1, true);
      scroll(1, true);
      scroll(1, false);
      event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
      scroll(-1, true);
      scroll(1, true);
      event.wheel.mouse_x = event.wheel.mouse_y = 16;
      scroll(-1, false);
      event.wheel.mouse_x = event.wheel.mouse_y = 80;
      scroll(-16, true);
      scroll(-1, false);
      scroll(16, true);
      scroll(1, false);
    }
  }
  SDL_Quit();
  return Report();
}
