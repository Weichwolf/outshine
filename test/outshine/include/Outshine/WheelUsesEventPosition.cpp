#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video starts for UI composition");
  {
    Engine engine;
    engine.setRoots({.Shipped = "src/assets"});
    CHECK(engine.drawsInto(Extent{128, 128}).has_value(), "offscreen target is configured");
    Scenario::Document scene;
    scene.WheelStepPx = 16;
    scene.Surfaces.push_back(
        {.Document = "<div id=\"scroll\"><div id=\"content\"></div></div>",
         .Style = "#scroll { width: 64px; height: 64px; overflow: scroll; }"
                  "#content { width: 32px; height: 128px; }",
         .Where = {.LeftFrac = 0.5, .TopFrac = 0.5, .WidthFrac = 0.5, .HeightFrac = 0.5}});
    const auto declared = engine.declare(scene);
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
      for (const double step : {-1.0,
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity()}) {
        auto invalid = scene;
        invalid.WheelStepPx = step;
        CHECK(!engine.declare(invalid), "invalid wheel step rejected before replacing scene");
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
      auto excessive = scene;
      excessive.WheelStepPx = std::numeric_limits<double>::max();
      CHECK(engine.declare(excessive).has_value(), "finite large wheel step remains representable");
      event.wheel.y = -2;
      CHECK(!engine.handleEvent(event), "overflow in pixel displacement rejected");
      CHECK(engine.declare(scene).has_value(), "normal configuration restored");
      scroll(1, false);
      scroll(-1, true);
      auto disabled = scene;
      disabled.WheelStepPx = 0;
      CHECK(engine.declare(disabled).has_value(), "zero step explicitly disables scrolling");
      scroll(-1, false);
    }
  }
  SDL_Quit();
  return Report();
}
