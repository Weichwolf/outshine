#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "SceneLighting.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 96;
constexpr size_t kPixels = size_t{kSide} * kSide;

struct Image {
  std::vector<float> Colour;
  std::vector<float> Normal;
  std::vector<float> Depth;
};

Geometry Surface(float code, SurfacePattern pattern) {
  Geometry geometry;
  Material material;
  material.Pattern = pattern;
  constexpr std::array<uint8_t, 4> white{255, 255, 255, 255};
  const auto image = geometry.addImage(1, 1, white);
  CHECK(image, "the actual textured mesh path has a neutral explicit image");
  if (!image) { return {}; }
  material.BaseColourMap.Image = *image;
  const auto surface = geometry.addSurface("pattern-control", material);
  CHECK(surface, "only the shared material pattern is varied");
  if (!surface) { return {}; }
  const auto part = geometry.addPart("sloped-surface", *surface);
  CHECK(part, "the finite sloped mesh has an owned material");
  if (!part) { return {}; }
  constexpr std::array<float, 12> positions{-1, -1, 0, 1, -1, 0, 1, 1, 1, -1, 1, 1};
  constexpr float ny = -.4472135955f;
  constexpr float nz = .894427191f;
  constexpr std::array<float, 12> normals{0, ny, nz, 0, ny, nz, 0, ny, nz, 0, ny, nz};
  const float end = code < 0 ? code : code + 2;
  CHECK(geometry.setPositions(*part, positions) && geometry.setNormals(*part, normals) &&
            geometry.setTexture(*part, std::array<float, 8>{code, 2, end, 2, end, 4, code, 4}) &&
            geometry.setTexture(*part, std::array<float, 8>{0, 0, 2, 0, 2, 2, 0, 2}, 1) &&
            geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "actual positions, normals, encoded facade coordinates and independent metric UV1 reach "
        "the mesh");
  return geometry;
}

Image Draw(float code, SurfacePattern pattern) {
  const auto geometry = Surface(code, pattern);
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kSide;
  declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  declaration.KeyLux = 10000;
  declaration.KeyElevationDeg = 35;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        error.empty() ? "the actual shared mesh scene opens" : error.c_str());
  if (!scene) { return {}; }
  auto eye = Viewpoint::LookAt({.EyeM = {{0, 0, 10}}, .AimM = {{0, 0, 0}}}, 0.0);
  CHECK(eye, "the pattern camera is valid");
  if (!eye) { return {}; }
  eye->Kind = CameraKind::Orthographic;
  eye->XMagM = eye->YMagM = 2;
  eye->ZNearM = .1;
  eye->ZFarM = 100;
  scene->Eye(*eye);
  CHECK(scene->Draw(error),
        error.empty() ? "initial geometry stands before the explicit light override"
                      : error.c_str());
  renderer.WaitForGpu();
  SceneLight light;
  light.Light.Intensity = 10000;
  light.Light.Direction = {{0, 0, -1}};
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error),
        "all controls have identical frontal lighting");
  CHECK(scene->Draw(error), error.empty() ? "the native pattern frame is drawn" : error.c_str());
  renderer.WaitForGpu();
  Image result;
  CHECK(renderer.ReadSceneLinear(result.Colour) == ReadState::Ready &&
            renderer.ReadShadingNormal(result.Normal) == ReadState::Ready &&
            renderer.ReadDepth(result.Depth) == ReadState::Ready,
        "shared colour, bump normal and actual depth are readable");
  return result;
}

void CheckPattern(float code, bool relief) {
  const auto plain = Draw(code, SurfacePattern::None);
  const auto pattern = Draw(code, SurfacePattern::FacadeMetres);
  CHECK(pattern.Colour.size() == kPixels * 4 && pattern.Normal.size() == kPixels * 4 &&
            pattern.Depth.size() == kPixels && plain.Colour.size() == pattern.Colour.size() &&
            plain.Normal.size() == pattern.Normal.size(),
        "all actual mesh outputs are complete");
  if (pattern.Colour.size() != kPixels * 4 || pattern.Normal.size() != kPixels * 4 ||
      pattern.Depth.size() != kPixels || plain.Colour.size() != pattern.Colour.size() ||
      plain.Normal.size() != pattern.Normal.size()) {
    return;
  }
  size_t visible = 0;
  size_t changedColour = 0;
  size_t changedNormal = 0;
  for (size_t i = 0; i < kPixels; ++i) {
    if (pattern.Depth[i] <= 0) { continue; }
    ++visible;
    for (size_t c = 0; c < 3; ++c) {
      CHECK(std::isfinite(pattern.Colour[i * 4 + c]) && std::isfinite(pattern.Normal[i * 4 + c]),
            "every actually shaded pattern component remains finite");
      if (std::abs(pattern.Colour[i * 4 + c] - plain.Colour[i * 4 + c]) > 1e-4f) {
        ++changedColour;
      }
      if (std::abs(pattern.Normal[i * 4 + c] - plain.Normal[i * 4 + c]) > 1e-4f) {
        ++changedNormal;
      }
    }
  }
  CHECK(visible > 0 && changedColour > 0,
        "the actual House or roof filter changes its own rendered signal");
  CHECK(pattern.Depth == plain.Depth, "the pattern preserves actual mesh geometry depth");
  CHECK(!relief || changedNormal > 0,
        "roof joint differentials reach the actual bump-normal consumer");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  CheckPattern(256, false);
  CheckPattern(-2, true);
  SDL_Quit();
  return Report();
}
