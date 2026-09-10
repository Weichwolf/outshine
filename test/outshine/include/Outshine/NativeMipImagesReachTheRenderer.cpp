#include <array>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  std::vector<uint8_t> pixels(512u * 512u * 4u, 255);
  for (size_t y = 0; y < 512; ++y) {
    for (size_t x = 0; x < 512; ++x) { pixels[(y * 512 + x) * 4] = (x + y) % 2 ? 255 : 0; }
  }
  const int image = geometry.addImage(512, 512, pixels);
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Unlit = true;
  material.BaseColourMap.Image = image;
  material.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  material.BaseColourMap.Sampler.Minify = Filter::Nearest;
  material.BaseColourMap.Sampler.Mip = MipFilter::Linear;
  material.BaseColourMap.Sampler.WrapU = Wrap::ClampToEdge;
  material.BaseColourMap.Sampler.WrapV = Wrap::ClampToEdge;
  const int part = geometry.addPart("quad", geometry.addSurface("colour", material).value());
  CHECK(geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "normals");
  CHECK(geometry.setTexture(part, std::array<float, 8>{0, 1, 1, 1, 1, 0, 0, 0}), "UVs");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "triangles");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {320, 320};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "native-map";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scenario.Views.push_back(view);
  std::vector<float> frame;
  if (!engine.drawsInto({320, 320}) || !engine.declare(scenario) || !engine.setGeometry(geometry)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  geometry.clear();
  if (!engine.assemble() || !engine.advance() || !engine.renderer().render({}) ||
      !engine.renderer().readPixels(Buffer::Linear, frame)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  CHECK(frame.size() == 320u * 320u * 4u, "linear frame has the declared dimensions");
  if (frame.size() != 320u * 320u * 4u) { return Report(); }
  float worst = 0;
  for (size_t y = 90; y < 230; ++y) {
    for (size_t x = 90; x < 230; ++x) {
      const size_t at = (y * 320 + x) * 4;
      if (frame[at + 1] <= 0.1f) {
        worst = 1;
        continue;
      }
      worst = std::max(worst, std::abs(frame[at] / frame[at + 1] - 0.5f));
    }
  }
  CHECK(worst < 1e-5f, "minified checker integrates half red against constant green");
  std::vector<float> repeat;
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeat),
        "repeat renders");
  CHECK(frame == repeat, "resident minified image repeats exactly");
  std::filesystem::create_directories("build/native-materials");
  CHECK(engine.renderer().saveScreenshot("build/native-materials/minified.png").has_value(),
        "native map PNG is written");
  return Report();
}
