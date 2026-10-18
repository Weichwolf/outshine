#include <Outshine.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <unistd.h>
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <cstdint>
#include <vector>
#include "Check.h"

namespace {
outshine::Scenario::Document Declaration(bool withView) {
  outshine::Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {32, 32};
  scene.Render.Outputs = {"surface"};
  if (withView) {
    outshine::Scenario::View view;
    view.Id = "camera-contract";
    view.Person = "first";
    view.Placement = outshine::Scenario::CameraPlacement::Local;
    view.Sees.PositionM = {{0, 0, 2}};
    view.Sees.setProjection(
        outshine::Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
    scene.Views.push_back(view);
  }
  return scene;
}

} // namespace

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video starts before creating targets");
  if (!initialized) { return Report(); }
  {
    Engine engine;
    const auto target = engine.swapChain();
    CHECK(target.extent().WidthPx == 0 && target.extent().HeightPx == 0,
          "internal default dimensions are not advertised as a configured target");
    CHECK(engine.setRenderTarget(Extent{32, 32}).has_value(), "configure an offscreen target");
    CHECK(engine.declare(Declaration(false)).has_value(),
          "an empty scene can be declared before a camera is prepared");
    auto renderer = engine.renderer();
    for (int attempt = 0; attempt < 2; ++attempt) {
      const auto result = renderer.render({});
      CHECK(!result && !result.error().empty(),
            "drawing without a camera or derivable bounds returns an error, not an assertion or "
            "false success");
    }
    std::vector<uint8_t> pixels;
    CHECK(!renderer.readPixels(pixels), "readback cannot bypass missing-camera preparation");
    const bool ready = engine.declare(Declaration(true)) && engine.assemble() && engine.advance();
    CHECK(ready, "an explicit view can be prepared after failed draw attempts");
    if (ready) {
      CHECK(!renderer.readPixels(pixels), "readback requires an explicitly rendered frame");
      CHECK(renderer.render({}).has_value(), "render prepared camera");
      CHECK(renderer.readPixels(pixels).has_value() && pixels.size() == 32u * 32u * 4u,
            "the same renderer recovers and produces a complete RGBA frame");
      constexpr std::array<Extent, 5> invalid = {{{-1, 32}, {0, 32}, {32, 0}, {32, -1}, {48, 32}}};
      for (const Extent frame : invalid) {
        CHECK(!renderer.render(frame),
              "partial, negative and mismatched extents cannot masquerade as the default target");
      }
      CHECK(renderer.render({}).has_value(),
            "invalid extent checks leave the prepared camera usable");
    }
  }
  SDL_Window *window = SDL_CreateWindow("Outshine camera contract", 32, 32, 0);
  CHECK(window != nullptr, "a real presentation target is created");
  if (window != nullptr) {
    {
      Engine engine;
      CHECK(engine.setRenderTarget(window).has_value(), "the Engine borrows the window");
      CHECK(engine.declare(Declaration(false)).has_value(),
            "a presentation scene starts without a bound camera");
      auto target = engine.swapChain();
      auto renderer = engine.renderer();
      for (int attempt = 0; attempt < 2; ++attempt) {
        const auto begun = renderer.beginFrame(target);
        CHECK(begun.has_value(), "frame setup remains independent of camera selection");
        if (begun) {
          CHECK(!renderer.endFrame(),
                "presentation propagates missing-camera failure before projection math");
        }
      }
      CHECK(!renderer.endFrame(), "the failed presentation did not leave a frame open");
      const bool ready = engine.declare(Declaration(true)) && engine.assemble() && engine.advance();
      CHECK(ready, "camera preparation recovers the existing window target");
      if (ready) {
        const auto begun = renderer.beginFrame(target);
        CHECK(begun.has_value(), "a new frame opens after recovery");
        if (begun) {
          CHECK(renderer.endFrame().has_value(), "the prepared camera presents successfully");
        }
      }
    }
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
  Covers("unconfigured target extent; missing-camera draw/readback/presentation failures and "
         "repeatability; "
         "explicit-view recovery; render extent validation; numerical camera conventions remain in "
         "their own suite");
  return Report();
}
