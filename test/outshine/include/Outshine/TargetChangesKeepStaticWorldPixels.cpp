#include <array>
#include <vector>

#include <Outshine.h>
#include <SDL3/SDL.h>
#include "Check.h"

namespace {
outshine::Scenario::Document Scene(outshine::Extent frame) {
  outshine::Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = frame;
  scene.Render.Outputs = {"sceneLinear"};
  outshine::Scenario::View view;
  view.Id = "static";
  view.Person = "first";
  view.Placement = outshine::Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(
      outshine::Camera::Ortho{.XMagM = 1, .YMagM = 1, .NearM = 0.1, .FarM = 10});
  scene.Views.push_back(view);
  return scene;
}

outshine::Geometry Triangle() {
  outshine::Geometry geometry;
  outshine::Material material;
  material.BaseColour = {{0.8F, 0.1F, 0.2F, 1}};
  material.Unlit = true;
  const auto surface = geometry.addSurface("red", material);
  const int part = geometry.addPart("triangle", *surface).value();
  (void)geometry.setPositions(part,
                              std::array<float, 9>{-0.8F, -0.6F, 0, 0.8F, -0.6F, 0, 0, 0.8F, 0});
  (void)geometry.setNormals(part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1});
  (void)geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2});
  return geometry;
}

[[nodiscard]] bool Draw(outshine::Engine &engine, std::vector<float> &pixels) {
  return engine.advance() && engine.renderer().render({}) &&
         engine.renderer().readPixels(outshine::Buffer::Linear, pixels);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr Extent kFirst{.WidthPx = 64, .HeightPx = 64};
  constexpr Extent kOther{.WidthPx = 96, .HeightPx = 64};
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes before configuring an engine target");
  if (!SDL_WasInit(SDL_INIT_VIDEO)) { return Report(); }

  struct VideoSession {
    ~VideoSession() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  };

  const VideoSession video;
  Engine engine;
  const Geometry triangle = Triangle();
  CHECK(engine.drawsInto(kFirst) && engine.declare(Scene(kFirst)) && engine.setGeometry(triangle) &&
            engine.assemble(),
        "a static native world prepares on the first target");
  std::vector<float> first;
  CHECK(Draw(engine, first) && first.size() == 64u * 64u * 4u,
        "the first target produces the expected static readback");
  CHECK(engine.drawsInto(kOther), "the unrelated wider offscreen target publishes");
  std::vector<float> wider;
  CHECK(Draw(engine, wider) && wider.size() == 96u * 64u * 4u,
        "the static world rebinds to the wider target");
  CHECK(engine.drawsInto(kFirst), "the original target extent publishes again");
  std::vector<float> restored;
  CHECK(Draw(engine, restored) && restored == first,
        "a static world preserves its original-target linear pixels across target switches");
  return Report();
}
