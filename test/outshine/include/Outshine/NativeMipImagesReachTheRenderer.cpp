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
  const auto accepted = [](const Result &result) {
    if (!result) { Unprepared(result.error().c_str()); }
    return result.has_value();
  };
  Geometry geometry;
  std::vector<uint8_t> pixels(512u * 512u * 4u, 255);
  for (size_t y = 0; y < 512; ++y) {
    for (size_t x = 0; x < 512; ++x) { pixels[(y * 512 + x) * 4] = (x + y) % 2 ? 255 : 0; }
  }
  const int image = *geometry.addImage(512, 512, pixels);
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Unlit = true;
  material.BaseColourMap.Image = image;
  material.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  material.BaseColourMap.Sampler.Minify = Filter::Nearest;
  material.BaseColourMap.Sampler.Mip = MipFilter::Linear;
  material.BaseColourMap.Sampler.WrapU = Wrap::ClampToEdge;
  material.BaseColourMap.Sampler.WrapV = Wrap::ClampToEdge;
  const int part =
      geometry.addPart("quad", geometry.addSurface("colour", material).value()).value();
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
  if (!accepted(engine.drawsInto({320, 320})) || !accepted(engine.declare(scenario)) ||
      !accepted(engine.setGeometry(geometry))) {
    return Report();
  }
  geometry.clear();
  if (!accepted(engine.assemble()) || !accepted(engine.advance()) ||
      !accepted(engine.renderer().render({})) ||
      !accepted(engine.renderer().readPixels(Buffer::Linear, frame))) {
    return Report();
  }
  CHECK(frame.size() == 320u * 320u * 4u, "linear frame has the declared dimensions");
  if (frame.size() != 320u * 320u * 4u) { return Report(); }
  constexpr float kLinearHalf = 0.5f;
  constexpr float kSrgbCodes = 255.0f;
  constexpr float kR16ReadbackError = 1.0f / 1024.0f;
  const auto encodedFromLinear = [](float value) {
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
  };
  const auto linearFromEncoded = [](float value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
  };
  const float storedSrgb = std::round(encodedFromLinear(kLinearHalf) * kSrgbCodes) / kSrgbCodes;
  const float expectedRed = linearFromEncoded(storedSrgb);
  float worst = 0;
  for (size_t y = 90; y < 230; ++y) {
    for (size_t x = 90; x < 230; ++x) {
      const size_t at = (y * 320 + x) * 4;
      if (frame[at + 1] <= 0.1f) {
        worst = 1;
        continue;
      }
      worst = std::max(worst, std::abs(frame[at] / frame[at + 1] - expectedRed));
    }
  }
  CHECK(worst <= kR16ReadbackError,
        "minified checker stores the quantized linear half red against constant green");
  std::vector<float> repeat;
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeat),
        "repeat renders");
  CHECK(frame == repeat, "resident minified image repeats exactly");
  std::filesystem::create_directories("build/native-materials");
  CHECK(engine.renderer().saveScreenshot("build/native-materials/minified.png").has_value(),
        "native map PNG is written");
  return Report();
}
