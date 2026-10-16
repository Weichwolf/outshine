#include <Outshine.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include "scene/PunctualLight.h"
#include "math/Mat4.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <ios>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <print>
#include <cstdio>
#include <string>
#include <vector>

namespace {
constexpr uint32_t kSide = 96;
constexpr float kLux = 1000;
constexpr double kReflectance = 0.5, kFillNormalCosine = 0.8;
constexpr size_t kChannels = 4;
constexpr float kFillEast = 0.6f;
constexpr double kSunBearingDeg = 90, kSunElevationDeg = 45;
constexpr double kNearM = 0.1, kFarM = 100;
constexpr double kReceiverStripM = 0.3, kShadowWestM = -1.2, kShadowEastM = -0.3;
constexpr float kLitThreshold = 80;
constexpr double kRelativeTolerance = 0.01;

outshine::Geometry World(size_t fills) {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  Material material;
  material.SpecularFactor = 0;
  const auto surface = geometry.addSurface("ideal diffuse", material);
  CHECK(surface.has_value(), "a zero-specular diffuse material is valid");
  if (!surface) { return {}; }
  const auto floor = geometry.addPart("receiver", *surface);
  const auto cube = geometry.addPart("caster", *surface);
  CHECK(floor && cube, "receiver and caster share an unchanged material");
  if (!floor || !cube) { return {}; }
  constexpr std::array<float, 12> floorPositions{-5, 0, -5, 5, 0, -5, 5, 0, 5, -5, 0, 5};
  constexpr std::array<float, 12> floorNormals{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  CHECK(geometry.setPositions(*floor, floorPositions) &&
            geometry.setNormals(*floor, floorNormals) &&
            geometry.setTriangles(*floor, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2}),
        "the receiver has a fixed analytical normal");
  constexpr std::array<float, 24> vertices{0.5f,  0,    -0.5f, 1.5f,  0,    -0.5f, 1.5f, 2,
                                           -0.5f, 0.5f, 2,     -0.5f, 0.5f, 0,     0.5f, 1.5f,
                                           0,     0.5f, 1.5f,  2,     0.5f, 0.5f,  2,    0.5f};
  constexpr std::array<uint32_t, 36> faces{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                                           3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
  CHECK(geometry.setPositions(*cube, vertices) && geometry.setTriangles(*cube, faces),
        "the closed caster supplies real solar occlusion");
  for (size_t index = 0; index < fills; ++index) {
    PunctualLight fill;
    fill.Colour = {{0, 0, 1}};
    fill.Intensity = kLux / static_cast<float>(fills);
    fill.Direction = {{kFillEast, -static_cast<float>(kFillNormalCosine), 0}};
    CHECK(geometry.addLamp("independent blue fill", fill, Mat4{}).has_value(),
          "authored fill lights precede the runtime key in the actual light list");
  }
  return geometry;
}

bool Accepted(const outshine::Result &result, const char *claim) {
  CHECK(result, claim);
  if (!result) { std::println(stderr, "{}", result.error()); }
  return result.has_value();
}

std::vector<float> Capture(size_t fills, const char *outputPrefix) {
  using namespace outshine;
  using namespace outshine::Test;
  auto geometry = World(fills);
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {.Width = kSide, .Height = kSide};
  scenario.Render.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = kLux;
  scenario.Lit.Key.BearingDeg = kSunBearingDeg;
  scenario.Lit.Key.ElevationDeg = kSunElevationDeg;
  scenario.Lit.ShadowRadiusM = 16;
  Scenario::View view;
  view.Id = "shadow receiver";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 10, 4}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0, 0, 0}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 3, .YMagM = 3, .NearM = kNearM, .FarM = kFarM});
  scenario.Views.push_back(view);
  std::vector<float> pixels;
  if (!Accepted(engine.setRenderTarget({.Width = kSide, .Height = kSide}),
                "public offscreen target opens") ||
      !Accepted(engine.declare(scenario), "public scenario declares its actual key and shadows") ||
      !Accepted(engine.setGeometry(geometry), "public geometry carries the authored fill lights") ||
      !Accepted(engine.assemble(), "public assembly binds the complete native light list") ||
      !Accepted(engine.advance(), "the public frame advances") ||
      !Accepted(engine.renderer().render({}), "the public frame renders") ||
      !Accepted(engine.renderer().readPixels(Buffer::Linear, pixels),
                "public linear readback completes")) {
    return {};
  }
  CHECK(pixels.size() == size_t{kSide} * kSide * kChannels,
        "the public image is complete before testing individual light contributions");
  if (outputPrefix != nullptr) {
    std::ofstream file(std::string(outputPrefix) + ".fill" + std::to_string(fills) + ".rgba32",
                       std::ios::binary);
    file.write(reinterpret_cast<const char *>(pixels.data()),
               static_cast<std::streamsize>(pixels.size() * sizeof(float)));
    CHECK(file.good(), "explicitly requested public capture is saved completely");
  }
  return pixels;
}

void Compare(const std::vector<float> &base, const std::vector<float> &withFill) {
  using namespace outshine::Test;
  if (base.size() != size_t{kSide} * kSide * kChannels || withFill.size() != base.size()) {
    return;
  }
  const double expectedBlue = kReflectance * kFillNormalCosine * kLux / std::numbers::pi;
  size_t shadowed = 0;
  size_t unshadowed = 0;
  double worstRelative = 0;
  double worstUnshadowedRelative = 0;
  double otherChannels = 0;
  for (uint32_t y = 0; y < kSide; ++y) {
    const double z = 3 - (static_cast<double>(y) + 0.5) * 6 / kSide;
    for (uint32_t x = 0; x < kSide; ++x) {
      const double east = (static_cast<double>(x) + 0.5) * 6 / kSide - 3;
      if (std::abs(z) > kReceiverStripM) { continue; }
      const size_t at = (size_t{y} * kSide + x) * kChannels;
      const double delta = withFill[at + 2] - base[at + 2];
      if (east < -2.0 && base[at] > kLitThreshold) {
        ++unshadowed;
        worstUnshadowedRelative =
            std::max(worstUnshadowedRelative, std::abs(delta - expectedBlue) / expectedBlue);
      }
      if (east < kShadowWestM || east > kShadowEastM || !(base[at] < 10)) { continue; }
      ++shadowed;
      worstRelative = std::max(worstRelative, std::abs(delta - expectedBlue) / expectedBlue);
      otherChannels = std::max(otherChannels,
                               static_cast<double>(std::abs(withFill[at] - base[at]) +
                                                   std::abs(withFill[at + 1] - base[at + 1])));
    }
  }
  CHECK(unshadowed > 32 && worstUnshadowedRelative < kRelativeTolerance,
        "authored blue fill really illuminates the unchanged unshadowed receiver");
  CHECK(shadowed > 32, "the assertion covers actual receiver pixels inside the key-light shadow");
  CHECK(worstRelative < kRelativeTolerance,
        "independent blue light survives the foreign sun shadow within HDR precision");
  CHECK(otherChannels == 0, "adding blue fill preserves the key-light red/green response exactly");
}
}

int main(int argc, char **argv) {
  if (argc > 2) { return 2; }
  const char *outputPrefix = argc == 2 ? argv[1] : nullptr;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the actual runtime contract");
  const auto baseline = Capture(0, outputPrefix);
  Compare(baseline, Capture(1, outputPrefix));
  Compare(baseline, Capture(15, outputPrefix));
  SDL_Quit();
  return Report();
}
