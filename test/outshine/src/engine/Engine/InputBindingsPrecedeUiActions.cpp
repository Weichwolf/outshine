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
  {
    Receiver receiver;
    Engine engine;
    engine.setInputHost(&receiver);
    Roots roots;
    roots.Shipped = "data";
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
    const auto declared = engine.declare(scene);
    CHECK(declared.has_value(), declared ? "UI declaration accepted" : declared.error().c_str());
    if (target && declared) {
      SDL_Event event{};
      event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
      event.button.button = SDL_BUTTON_LEFT;
      event.button.x = event.button.y = kInsidePx;
      auto handled = engine.handleEvent(event);
      CHECK(handled && *handled && receiver.Names == std::vector<std::string>{"clicked"},
            "unbound mouse reaches UI even while another input is bound");
      receiver.Names.clear();
      scene.Input.push_back({.Event = "MouseLeft", .Action = "bound"});
      CHECK(engine.declare(scene).has_value(), "mouse binding replaces routing policy");
      handled = engine.handleEvent(event);
      CHECK(handled && *handled && receiver.Names == std::vector<std::string>{"bound"},
            "explicit mouse binding prevents a second UI action");
      receiver.Names.clear();
      receiver.Accept = false;
      handled = engine.handleEvent(event);
      CHECK(handled && !*handled && receiver.Names == std::vector<std::string>{"bound"},
            "refused binding cannot fall through to UI");
      receiver.Accept = true;
      scene.Input.clear();
      CHECK(engine.declare(scene).has_value(), "bindings can be removed with the UI retained");
      receiver.Names.clear();
      handled = engine.handleEvent(event);
      CHECK(handled && *handled && receiver.Names == std::vector<std::string>{"clicked"},
            "removing binding restores UI dispatch");
      engine.setInputHost(static_cast<Host *>(nullptr));
      CHECK(!engine.handleEvent(event), "UI action without host returns a processing error");
      event.button.x = event.button.y = kOutsidePx;
      handled = engine.handleEvent(event);
      CHECK(handled && !*handled, "outside hit is unhandled even after a processing error");
    }
  }
  SDL_Quit();
  return Report();
}
