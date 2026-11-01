#include <Engine.h>
#include "Check.h"
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace {
class Receiver final : public outshine::Host {
public:
  std::vector<std::string> Names;
  bool Accept = true;

  bool calls(std::string_view name,
             [[maybe_unused]] std::span<const outshine::Argument> args) override {
    Names.emplace_back(name);
    return Accept;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr float kInsidePx = 16;
  constexpr float kOutsidePx = 96;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes for an actual UI surface");
  for (const int policy : {0, 1, 2}) {
    Receiver receiver;
    Engine engine;
    engine.setInputHost(&receiver);
    Roots roots;
    roots.BuiltinDataDirectory = "data";
    CHECK(engine.setRoots(roots), "UI roots configured");
    const auto target = engine.setRenderTarget(Extent{.WidthPx = 64, .HeightPx = 64});
    CHECK(target.has_value(), target ? "offscreen UI target ready" : target.error().c_str());
    Scenario::Document scene;
    scene.Surfaces.push_back(
        {.Document = "<div data-action=\"if(ready()) clicked()\"></div>",
         .Style = "div { width: 64px; height: 64px; }",
         .Programme = "const ready = () => [1,2,3].reduce((sum,x) => sum+x,0) === 6;",
         .Where = {}});
    scene.Input = {{.Event = "KeyW", .Action = "key"}};
    if (policy == 1) { scene.Input.push_back({.Event = "MouseLeft", .Action = "bound"}); }
    if (policy == 2) { scene.Input.clear(); }
    const auto declared = engine.loadScenario(scene);
    CHECK(declared.has_value(), declared ? "UI declaration accepted" : declared.error().c_str());
    if (target && declared) {
      SDL_Event event{};
      event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
      event.button.button = SDL_BUTTON_LEFT;
      event.button.x = event.button.y = kInsidePx;
      auto handled = engine.handleEvent(event);
      const std::string expected = policy == 1 ? "bound" : "clicked";
      CHECK(handled && *handled && receiver.Names == std::vector<std::string>{expected},
            "an explicit mouse binding precedes UI; otherwise the hit dispatches once");
      if (policy == 1) {
        receiver.Names.clear();
        receiver.Accept = false;
        handled = engine.handleEvent(event);
        CHECK(handled && !*handled && receiver.Names == std::vector<std::string>{"bound"},
              "a refused binding cannot fall through to UI");
      }
      receiver.Accept = true;
      engine.setInputHost(static_cast<Host *>(nullptr));
      CHECK(!engine.handleEvent(event), "a dispatched action without a host is an error");
      event.button.x = event.button.y = kOutsidePx;
      if (policy != 1) {
        handled = engine.handleEvent(event);
        CHECK(handled && !*handled, "outside UI hits are unhandled after a processing error");
      }
    }
  }
  SDL_Quit();
  return Report();
}
