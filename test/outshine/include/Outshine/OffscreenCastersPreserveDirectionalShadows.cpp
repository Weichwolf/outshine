#include <Outshine.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <numbers>
#include <print>
#include <string>
#include <vector>

namespace {
constexpr int kSide = 32;
constexpr size_t kChannels = 4;
constexpr float kLux = 10000;
constexpr double kReflectance = 0.5;
constexpr double kShadowRadiusM = 16384;
constexpr float kInsideM = 16000, kOutsideM = 40000;
constexpr double kNearM = 0.1, kFarM = 2000;
constexpr double kBearingDeg = 180;
constexpr double kRelativeTolerance = 0.01;
constexpr float kMaskedAlpha = 0.75f;
constexpr double kViewHalfWidthM = 60, kEyeDepthM = 100;
enum class Caster { None, Inside, Outside, Crossing, Behind };

outshine::Geometry World(Caster caster, outshine::AlphaMode alpha) {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  Material receiver;
  receiver.SpecularFactor = 0;
  constexpr auto reflectance = static_cast<float>(kReflectance);
  receiver.BaseColour = {{reflectance, reflectance, reflectance, 1}};
  receiver.DoubleSided = true;
  const auto surface = geometry.addSurface("diffuse receiver", receiver);
  CHECK(surface.has_value(), "receiver material is valid");
  if (!surface) { return {}; }
  const auto floor = geometry.addPart("receiver", *surface);
  CHECK(floor.has_value(), "receiver exists");
  if (!floor) { return {}; }
  constexpr std::array<float, 12> positions{-80, -80, 0, 80, -80, 0, 80, 80, 0, -80, 80, 0};
  constexpr std::array<float, 12> normals{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
  CHECK(geometry.setPositions(*floor, positions) && geometry.setNormals(*floor, normals) &&
            geometry.setTriangles(*floor, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "unchanged receiver faces the directional sun");
  if (caster == Caster::None) { return geometry; }
  Material material;
  material.Alpha = alpha;
  material.BaseColour[3] = alpha == AlphaMode::Masked ? kMaskedAlpha : 1;
  material.DoubleSided = true;
  const auto castSurface = geometry.addSurface("opaque coverage", material);
  CHECK(castSurface.has_value(), "caster material is valid");
  if (!castSurface) { return {}; }
  const auto part = geometry.addPart("offscreen caster", *castSurface);
  CHECK(part.has_value(), "offscreen caster exists");
  if (!part) { return {}; }
  float left = kInsideM;
  if (caster == Caster::Outside) { left = kOutsideM; }
  if (caster == Caster::Behind) { left = -kOutsideM; }
  const float right = caster == Caster::Crossing ? kOutsideM : left;
  constexpr float halfWidth = 128;
  constexpr float halfDepth = 2;
  const std::array<float, 24> box{
      -halfWidth, -halfWidth, left - halfDepth,  halfWidth,  -halfWidth, right - halfDepth,
      halfWidth,  halfWidth,  right - halfDepth, -halfWidth, halfWidth,  left - halfDepth,
      -halfWidth, -halfWidth, left + halfDepth,  halfWidth,  -halfWidth, right + halfDepth,
      halfWidth,  halfWidth,  right + halfDepth, -halfWidth, halfWidth,  left + halfDepth};
  constexpr std::array<uint32_t, 36> triangles{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                               0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
                                               0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
  CHECK(geometry.setPositions(*part, box) && geometry.setTriangles(*part, triangles),
        "closed offscreen caster is unchanged except for light depth");
  return geometry;
}

bool Accepted(const outshine::Result &result, const char *claim) {
  CHECK(result, claim);
  if (!result) { std::println(stderr, "{}", result.error()); }
  return result.has_value();
}

std::vector<float> CaptureReceiver(Caster caster, outshine::AlphaMode alpha, const char *prefix) {
  using namespace outshine;
  auto geometry = World(caster, alpha);
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {.WidthPx = kSide, .HeightPx = kSide};
  scenario.Render.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = kLux;
  scenario.Lit.Key.BearingDeg = kBearingDeg;
  scenario.Lit.Key.ElevationDeg = 0;
  scenario.Lit.ShadowRadiusM = kShadowRadiusM;
  Scenario::View view;
  view.Id = "unchanged receiver";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, kEyeDepthM}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0, 0, 0}};
  view.Sees.setProjection(Camera::Ortho{
      .XMagM = kViewHalfWidthM, .YMagM = kViewHalfWidthM, .NearM = kNearM, .FarM = kFarM});
  scenario.Views.push_back(view);
  std::vector<float> pixels;
  if (!Accepted(engine.setRenderTarget({.WidthPx = kSide, .HeightPx = kSide}), "target opens") ||
      !Accepted(engine.declare(scenario), "the same light and camera are declared") ||
      !Accepted(engine.setGeometry(geometry), "geometry is accepted") ||
      !Accepted(engine.assemble(), "the public scene assembles") ||
      !Accepted(engine.advance(), "the public frame advances") ||
      !Accepted(engine.renderer().render({}), "the public receiver renders") ||
      !Accepted(engine.renderer().readPixels(Buffer::Linear, pixels), "linear image reads back")) {
    return {};
  }
  CHECK(pixels.size() == size_t{kSide} * kSide * kChannels, "the public image is complete");
  if (prefix != nullptr) {
    const std::string path = std::string(prefix) + "." + std::to_string(static_cast<int>(alpha)) +
                             "." + std::to_string(static_cast<int>(caster)) + ".rgba32";
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(pixels.data()),
               static_cast<std::streamsize>(pixels.size() * sizeof(float)));
    CHECK(file.good(), "explicitly requested diagnostic is saved completely");
  }
  return pixels;
}

void Verify(const std::vector<float> &clear,
            const std::vector<float> &inside,
            const std::vector<float> &outside,
            const std::vector<float> &crossing,
            const std::vector<float> &behind) {
  const size_t elements = size_t{kSide} * kSide * kChannels;
  if (clear.size() != elements || inside.size() != elements || outside.size() != elements ||
      crossing.size() != elements || behind.size() != elements) {
    return;
  }
  double worstRelative = 0;
  constexpr int first = 12;
  constexpr int last = 20;
  const double expected = kReflectance * kLux / std::numbers::pi;
  for (int y = first; y < last; ++y) {
    for (int x = first; x < last; ++x) {
      const size_t at = (size_t(y) * kSide + size_t(x)) * kChannels;
      for (size_t channel = 0; channel < 3; ++channel) {
        const double removed = clear[at + channel] - inside[at + channel];
        worstRelative = std::max(worstRelative, std::abs(removed - expected) / expected);
      }
    }
  }
  CHECK(worstRelative < kRelativeTolerance,
        "the offscreen interior control removes the analytical direct Lambert contribution");
  CHECK(inside == outside,
        "moving a hidden caster towards the sun beyond light-near preserves its shadow");
  CHECK(inside == crossing,
        "a hidden caster crossing light-near preserves its full shadow footprint");
  CHECK(clear == behind, "a caster behind the receiver and light-far adds no false shadow");
}
}

int main(int argc, char **argv) {
  if (argc > 2) { return 2; }
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for actual offscreen shadow receivers");
  const char *prefix = argc == 2 ? argv[1] : nullptr;
  const auto clear = CaptureReceiver(Caster::None, AlphaMode::Opaque, prefix);
  for (const auto alpha : {AlphaMode::Opaque, AlphaMode::Masked}) {
    const auto inside = CaptureReceiver(Caster::Inside, alpha, prefix);
    const auto outside = CaptureReceiver(Caster::Outside, alpha, prefix);
    const auto crossing = CaptureReceiver(Caster::Crossing, alpha, prefix);
    const auto behind = CaptureReceiver(Caster::Behind, alpha, prefix);
    Verify(clear, inside, outside, crossing, behind);
  }
  SDL_Quit();
  return Report();
}
