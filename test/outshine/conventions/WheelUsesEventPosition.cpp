#include <Outshine.h>
#include "Check.h"

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
