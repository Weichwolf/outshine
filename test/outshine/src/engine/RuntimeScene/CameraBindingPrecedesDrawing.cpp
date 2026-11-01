#include <cstddef>
#include <Engine.h>
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
  scene.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
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

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckOffscreen(bool withView) {
  Engine engine;
  CHECK(engine.frameExtent().WidthPx == 0 && engine.frameExtent().HeightPx == 0,
        "unconfigured target has no advertised dimensions");
  CHECK(engine.setRenderTarget(Extent{32, 32}), "offscreen target opens");
  CHECK(engine.loadScenario(Declaration(withView)), "the camera fixture starts once");
  auto renderer = engine.renderer();
  std::vector<uint8_t> pixels;
  if (!withView) {
    for (int attempt = 0; attempt < 2; ++attempt) {
      const auto rendered = renderer.render({});
      CHECK(!rendered && !rendered.error().empty(), "missing camera produces a draw error");
    }
    CHECK(!renderer.readPixels(pixels), "readback cannot bypass a missing camera");
    return;
  }
  CHECK(engine.advance(), "the declared camera prepares");
  CHECK(!renderer.readPixels(pixels), "readback requires an explicitly rendered frame");
  CHECK(renderer.render({}) && renderer.readPixels(pixels) &&
            pixels.size() == size_t{32} * 32u * 4u,
        "a prepared camera produces a complete RGBA frame");
  constexpr std::array<Extent, 5> invalid = {{{.WidthPx = -1, .HeightPx = 32},
                                              {.WidthPx = 0, .HeightPx = 32},
                                              {.WidthPx = 32, .HeightPx = 0},
                                              {.WidthPx = 32, .HeightPx = -1},
                                              {.WidthPx = 48, .HeightPx = 32}}};
  for (const Extent frame : invalid) {
    CHECK(!renderer.render(frame), "partial, negative and mismatched extents are rejected");
  }
  CHECK(renderer.render({}), "invalid extents leave the prepared camera usable");
}

void CheckPresentation(SDL_Window *window, bool withView) {
  Engine engine;
  CHECK(engine.setRenderTarget(window), "a fresh engine borrows the presentation window");
  CHECK(engine.loadScenario(Declaration(withView)), "the presentation fixture starts once");
  auto renderer = engine.renderer();
  if (withView) { CHECK(engine.advance(), "the declared camera prepares for presentation"); }
  for (int attempt = 0; attempt < 2; ++attempt) {
    const auto begun = renderer.beginFrame();
    CHECK(begun, "frame acquisition is independent of camera selection");
    if (!begun) { continue; }
    const auto ended = renderer.endFrame();
    CHECK(ended.has_value() == withView, "presentation requires a prepared camera");
  }
  CHECK(!renderer.endFrame(), "successful and failed presentation both close their frame");
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for camera tests");
  CheckOffscreen(false);
  CheckOffscreen(true);
  SDL_Window *window = SDL_CreateWindow("Outshine camera contract", 32, 32, 0);
  CHECK(window != nullptr, "a presentation window opens");
  if (window != nullptr) {
    CheckPresentation(window, false);
    CheckPresentation(window, true);
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
  return Report();
}
