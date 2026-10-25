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
    auto target = first.swapChain();
    auto foreign = second.swapChain();
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
    const auto refused = copied.beginFrame(foreign);
    CHECK(!refused && refused.error().contains("another engine"),
          "a copied renderer refuses a foreign target by owner identity, not dimensions");
    CHECK(!render.endFrame(), "refusal did not begin a frame on the first Engine");
    CHECK(!other.endFrame(), "refusal did not begin a frame on the second Engine");
    CHECK(render.beginFrame(target).has_value(), "the first owner remains usable");
    CHECK(!render.beginFrame(target), "an already open frame rejects a repeated begin");
    CHECK(!copied.beginFrame(target), "facade copies cannot nest their owner's frame");
    CHECK(!first.setRenderTarget({.WidthPx = 48, .HeightPx = 32}),
          "rejected nested begin preserves the original open scope");
    CHECK(copied.endFrame().has_value(), "copied facades share their owner's frame state");
    CHECK(!render.endFrame(), "exactly one close consumes the original frame");
    CHECK(other.beginFrame(foreign).has_value(), "the second owner remains usable");
    CHECK(other.endFrame().has_value(), "second frame ends independently");
    CHECK(first.setRenderTarget({.WidthPx = 48, .HeightPx = 32}).has_value(),
          "target can change without moving the Engine");
    CHECK(target.extent().WidthPx == 48 && foreign.extent().WidthPx == 32,
          "borrowed targets observe only their own Engine's changes");
    CHECK(copied.beginFrame(target).has_value(),
          "retained facade remains valid after target change");
    CHECK(render.endFrame().has_value(), "frame closes after target change");
  }
  SDL_Quit();
  Covers("stable nonmovable owners; cross-owner refusal without frame "
         "mutation; facade copies and target changes preserve owner identity");
  return Report();
}
