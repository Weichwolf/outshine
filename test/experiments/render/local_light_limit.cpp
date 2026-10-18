#include <Outshine.h>
#include <scenario/Scenario.h>
#include <scene/Geometry.h>
#include <scene/PunctualLight.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <vector>

namespace {
constexpr uint32_t kWidth = 64, kHeight = 64;
constexpr float kTotalCandela = 1000, kRangeM = 32, kSunElevationDeg = 90;

bool Accepted(const outshine::Result &result, const char *step) {
  if (result) { return true; }
  std::println(stderr, "step={} error={}", step, result.error());
  return false;
}

bool Capture(size_t points) {
  using namespace outshine;
  Geometry geometry;
  Material material;
  material.SpecularFactor = 0;
  const auto surface = geometry.addSurface("fixed diffuse receiver", material);
  if (!surface) { return false; }
  const auto part = geometry.addPart("receiver", *surface);
  if (!part) { return false; }
  if (!geometry.setPositions(*part,
                             std::array<float, 12>{-2, 0, -2, 2, 0, -2, 2, 0, 2, -2, 0, 2}) ||
      !geometry.setNormals(*part, std::array<float, 12>{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}) ||
      !geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2})) {
    return false;
  }
  for (size_t index = 0; index < points; ++index) {
    PunctualLight light;
    light.Kind = LightKind::Point;
    light.Position = {0, 2, 0};
    light.RangeM = kRangeM;
    light.Intensity = kTotalCandela / static_cast<float>(points);
    if (!geometry.addLamp("coincident point control", light, Mat4{})) { return false; }
  }
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {.WidthPx = kWidth, .HeightPx = kHeight};
  scenario.Render.Outputs = {"sceneLinear"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = 1;
  scenario.Lit.Key.ElevationDeg = kSunElevationDeg;
  Scenario::View view;
  view.Id = "receiver";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {0, 3, 3};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {0, 0, 0};
  scenario.Views.push_back(view);
  Engine engine;
  std::vector<float> pixels;
  if (!Accepted(engine.setRenderTarget({.WidthPx = kWidth, .HeightPx = kHeight}), "target") ||
      !Accepted(engine.declare(scenario), "declare") ||
      !Accepted(engine.setGeometry(geometry), "geometry") ||
      !Accepted(engine.assemble(), "assemble") || !Accepted(engine.advance(), "advance") ||
      !Accepted(engine.renderer().render({}), "render") ||
      !Accepted(engine.renderer().readPixels(Buffer::Linear, pixels), "readback")) {
    std::println("point_lights={} total_emitters={} success=false", points, points + 1);
    return false;
  }
  if (pixels.size() != size_t{kWidth} * kHeight * 4) { return false; }
  const auto maximum = std::ranges::max(pixels);
  const bool complete = pixels.size() == size_t{kWidth} * kHeight * 4 && maximum > 1;
  std::println("point_lights={} total_emitters={} pixels={} maximum={} success={}",
               points,
               points + 1,
               pixels.size() / 4,
               maximum,
               complete);
  return complete;
}
}

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "{}", SDL_GetError());
    return 2;
  }
  const bool sixteen = Capture(15);
  const bool seventeen = Capture(16);
  SDL_Quit();
  return sixteen && seventeen ? 0 : 1;
}
