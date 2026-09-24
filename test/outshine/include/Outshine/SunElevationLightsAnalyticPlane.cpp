#include <Outshine.h>
#include <scenario/Scenario.h>
#include <scene/Geometry.h>

#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 128;

outshine::Geometry Receiver() {
  outshine::Geometry geometry;
  outshine::Material material;
  material.BaseColour = {{0.8f, 0.8f, 0.8f, 1.0f}};
  material.Roughness = 1.0f;
  const int part =
      geometry.addPart("receiver", geometry.addSurface("chalk", material).value()).value();
  CHECK(geometry.setPositions(
            part,
            std::array<float, 12>{
                -2.0f, 0.0f, -2.0f, 2.0f, 0.0f, -2.0f, 2.0f, 0.0f, 2.0f, -2.0f, 0.0f, 2.0f}),
        "plane positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}),
        "plane normals");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2}), "plane triangles");
  return geometry;
}

std::vector<float>
Capture(outshine::Engine &engine, const outshine::Geometry &geometry, double elevationDeg) {
  outshine::Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {kWidth, kHeight};
  scenario.Render.Outputs = {"sceneLinear"};
  scenario.Render.Exposure = 0.00005208;
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = 40000.0;
  scenario.Lit.Key.ElevationDeg = elevationDeg;
  scenario.Lit.Key.BearingDeg = 180.0;
  outshine::Scenario::View view;
  view.Id = "receiver";
  view.Person = "first";
  view.Placement = outshine::Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0.0, 2.0, 4.0}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0.0, 0.0, 0.0}};
  view.Sees.FovDeg = 55.0;
  scenario.Views.push_back(view);
  std::vector<float> pixels;
  if (!engine.declare(scenario) || !engine.setGeometry(geometry) || !engine.assemble() ||
      !engine.advance() || !engine.renderer().render({}) ||
      !engine.renderer().readPixels(outshine::Buffer::Linear, pixels)) {
    outshine::Test::Unprepared("analytic receiver could not be rendered");
  }
  return pixels;
}

double Average(const std::vector<float> &pixels, int from, int until) {
  double sum = 0.0;
  int count = 0;
  for (int row = from; row < until; ++row) {
    for (int column = kWidth / 4; column < kWidth * 3 / 4; ++column) {
      const size_t at = (static_cast<size_t>(row) * kWidth + column) * 4;
      if (at + 2 >= pixels.size()) { continue; }
      sum += 0.2126 * pixels[at] + 0.7152 * pixels[at + 1] + 0.0722 * pixels[at + 2];
      ++count;
    }
  }
  return count > 0 ? sum / count : 0.0;
}

}

int main() {
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  const outshine::Geometry receiver = Receiver();
  outshine::Engine engine;
  if (!engine.setRenderTarget({kWidth, kHeight})) {
    Unprepared("analytic target could not be created");
    return Report();
  }
  const auto low = Capture(engine, receiver, 5.0);
  const auto middle = Capture(engine, receiver, 30.0);
  const auto high = Capture(engine, receiver, 75.0);
  const auto lowAgain = Capture(engine, receiver, 5.0);
  CHECK(!low.empty() && low.size() == middle.size() && middle.size() == high.size() &&
            high.size() == lowAgain.size(),
        "analytic receiver renders at every sun elevation");
  if (low.empty() || low.size() != middle.size() || middle.size() != high.size() ||
      high.size() != lowAgain.size()) {
    return Report();
  }
  CHECK(low == lowAgain, "returning to the same analytic scene reproduces every linear pixel");
  const double lowGround = Average(low, kHeight / 2, kHeight * 3 / 4);
  const double middleGround = Average(middle, kHeight / 2, kHeight * 3 / 4);
  const double highGround = Average(high, kHeight / 2, kHeight * 3 / 4);
  std::printf("plane sun 5 %.4f 30 %.4f 75 %.4f\n", lowGround, middleGround, highGround);
  CHECK(highGround > middleGround && middleGround > lowGround,
        "horizontal diffuse receiver brightens monotonically with sun elevation");
  if (lowGround > 0.0) {
    const auto sine = [](double degrees) { return std::sin(degrees * std::numbers::pi / 180.0); };
    CHECK(std::abs((middleGround / lowGround) / (sine(30.0) / sine(5.0)) - 1.0) < 0.08 &&
              std::abs((highGround / lowGround) / (sine(75.0) / sine(5.0)) - 1.0) < 0.08,
          "linear receiver luminance tracks the geometric sine of sun elevation");
  }
  SDL_Quit();
  return Report();
}
