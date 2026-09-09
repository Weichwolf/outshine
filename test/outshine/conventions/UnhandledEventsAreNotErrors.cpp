#include <Outshine.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  const auto missingView = engine.setView("absent");
  CHECK(!missingView, "fixture establishes an earlier unrelated error");
  for (const auto type :
       {SDL_EVENT_QUIT, SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_KEY_DOWN, SDL_EVENT_MOUSE_BUTTON_DOWN}) {
    SDL_Event event{};
    event.type = type;
    const auto handled = engine.handleEvent(event);
    CHECK(handled && !*handled,
          "event without an active scene is unhandled, not the previous error");
  }
  return Report();
}
