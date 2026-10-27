#include <Engine.h>
#include <SDL3/SDL.h>
#include <string>
#include <type_traits>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(!std::is_copy_constructible_v<Engine> && !std::is_move_constructible_v<Engine> &&
            !std::is_move_assignable_v<Engine>,
        "engine ownership remains stable across borrowed renderer facades");
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video is initialized before configuring GPU targets");
  if (!initialized) { return Report(); }
  {
    Engine first;
    Engine second;
    auto render = first.renderer();
    CHECK(render.flushAndWait().has_value(), "waiting without a scene is a successful no-op");
    auto copied = render;
    auto other = second.renderer();
    CHECK(first.setRenderTarget({.WidthPx = 32, .HeightPx = 32}).has_value(),
          "first offscreen target");
    CHECK(second.setRenderTarget({.WidthPx = 32, .HeightPx = 32}).has_value(),
          "second target has identical dimensions");
    Scenario::Document scenario;
    scenario.Render.Declared = true;
    scenario.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
    scenario.Render.Outputs = {"sceneLinear"};
    CHECK(first.declare(scenario).has_value(), "first scenario");
    CHECK(second.declare(scenario).has_value(), "second scenario");
    CHECK(render.flushAndWait().has_value(), "initialized owner can wait for GPU work");
    CHECK(!render.endFrame(), "the first Engine begins with a closed frame");
    CHECK(!other.endFrame(), "the second Engine begins with an independent closed frame");
    CHECK(render.beginFrame().has_value(), "the first owner remains usable");
    CHECK(!render.beginFrame(), "an already open frame rejects a repeated begin");
    CHECK(!copied.beginFrame(), "facade copies cannot nest their owner's frame");
    CHECK(!other.endFrame(), "beginning the first frame cannot open the second owner");
    CHECK(other.beginFrame().has_value(), "another Engine can hold an independent frame");
    CHECK(!other.beginFrame(), "the second frame also refuses its own nested begin");
    CHECK(!first.setRenderTarget({.WidthPx = 48, .HeightPx = 32}),
          "rejected nested begin preserves the original open scope");
    CHECK(copied.endFrame().has_value(), "copied facades share their owner's frame state");
    CHECK(!render.endFrame(), "exactly one close consumes the original frame");
    CHECK(!second.setRenderTarget({.WidthPx = 48, .HeightPx = 32}),
          "closing the first frame cannot release the second Engine scope");
    CHECK(other.endFrame().has_value(), "second frame ends independently");
    CHECK(first.setRenderTarget({.WidthPx = 48, .HeightPx = 32}).has_value(),
          "target can change without moving the Engine");
    CHECK(first.frameExtent().WidthPx == 48 && second.frameExtent().WidthPx == 32,
          "target state changes remain local to their Engine");
    CHECK(copied.beginFrame().has_value(), "retained facade remains valid after target change");
    CHECK(render.endFrame().has_value(), "frame closes after target change");
  }
  SDL_Quit();
  Covers("stable owners; independent simultaneous frame scopes; copied renderer references, "
         "nested begin refusal and target changes preserve per-Engine state");
  return Report();
}
