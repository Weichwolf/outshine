#include <Outshine.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <type_traits>
#include "Check.h"
#include "Shell.h"

static_assert(!std::is_copy_constructible_v<outshine::Engine>);
static_assert(!std::is_move_constructible_v<outshine::Engine>);
static_assert(!std::is_move_assignable_v<outshine::Engine>);

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string compilation;
  CHECK(Run("printf '#include <Outshine.h>\\n' | c++ -std=c++23 -Iinclude "
            "$(pkg-config --cflags sdl3) -x c++ -fsyntax-only - 2>&1",
            compilation) == 0,
        "the public facade compiles without internal include directories");
  std::printf("%s", compilation.c_str());
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
    CHECK(first.drawsInto({32, 32}).has_value(), "first offscreen target");
    CHECK(second.drawsInto({32, 32}).has_value(), "second target has identical dimensions");
    Scenario::Document scenario;
    scenario.Render.Declared = true;
    scenario.Render.Frame = {32, 32};
    scenario.Render.Outputs = {"sceneLinear"};
    CHECK(first.declare(scenario).has_value(), "first scenario");
    CHECK(second.declare(scenario).has_value(), "second scenario");
    CHECK(render.flushAndWait().has_value(), "initialized owner can wait for GPU work");
    const auto refused = copied.beginFrame(foreign);
    CHECK(!refused && refused.error().find("another engine") != std::string::npos,
          "a copied renderer refuses a foreign target by owner identity, not dimensions");
    CHECK(!render.endFrame(), "refusal did not begin a frame on the first Engine");
    CHECK(!other.endFrame(), "refusal did not begin a frame on the second Engine");
    CHECK(render.beginFrame(target).has_value(), "the first owner remains usable");
    CHECK(!render.beginFrame(target), "an already open frame rejects a repeated begin");
    CHECK(!copied.beginFrame(target), "facade copies cannot nest their owner's frame");
    CHECK(!first.drawsInto({48, 32}), "rejected nested begin preserves the original open scope");
    CHECK(copied.endFrame().has_value(), "copied facades share their owner's frame state");
    CHECK(!render.endFrame(), "exactly one close consumes the original frame");
    CHECK(other.beginFrame(foreign).has_value(), "the second owner remains usable");
    CHECK(other.endFrame().has_value(), "second frame ends independently");
    CHECK(first.drawsInto({48, 32}).has_value(), "target can change without moving the Engine");
    CHECK(target.extent().WidthPx == 48 && foreign.extent().WidthPx == 32,
          "borrowed targets observe only their own Engine's changes");
    CHECK(copied.beginFrame(target).has_value(),
          "retained facade remains valid after target change");
    CHECK(render.endFrame().has_value(), "frame closes after target change");
  }
  {
    Engine engine;
    Scenario::Document deferred;
    Scenario::Asset broken;
    broken.Kind = "gltf";
    const auto missing = std::filesystem::temp_directory_path() /
                         ("outshine-frame-preflight-" + std::to_string(SDL_GetTicksNS()) + ".gltf");
    CHECK(!std::filesystem::exists(missing), "the deferred asset fixture is absent");
    broken.Uri = missing.string();
    deferred.Assets.push_back(broken);
    CHECK(engine.declare(deferred).has_value(), "scene setup can be deferred without a target");
    CHECK(engine.drawsInto({32, 32}).has_value(),
          "configure target without loading the deferred asset");
    auto renderer = engine.renderer();
    const auto wrongSize = renderer.render({48, 32});
    CHECK(!wrongSize && wrongSize.error().find("canvas") != std::string::npos,
          "extent mismatch is rejected before failing deferred scene setup");
    const auto correctSize = renderer.render({32, 32});
    CHECK(!correctSize &&
              correctSize.error().find(missing.filename().string()) != std::string::npos,
          "correct extent reaches the independently missing deferred asset");
  }
  SDL_Quit();
  Covers("stable nonmovable owners; public-header isolation; cross-owner refusal without frame "
         "mutation; facade copies and target changes preserve owner identity");
  return Report();
}
