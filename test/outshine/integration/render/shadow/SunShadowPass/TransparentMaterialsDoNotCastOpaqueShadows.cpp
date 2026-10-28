#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
void CheckCaster(outshine::Material material, bool casts) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  material.DoubleSided = true;
  material.Emission = {{1, .25f, .1f}};
  Geometry geometry;
  const auto surface = geometry.addSurface("caster", material);
  CHECK(surface.has_value(), "declared caster material is accepted");
  if (!surface) { return; }
  const auto part = geometry.addPart("card", *surface);
  CHECK(part &&
            geometry.setPositions(
                *part, std::array<float, 12>{-80, -80, 0, 80, -80, 0, 80, 80, 0, -80, 80, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "source geometry exists independently from its alpha policy");
  if (!part) { return; }
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear", "sceneDepth", "shadowAtlas"};
  declaration.ShadowRadiusM = 16384;
  declaration.KeyLux = 100;
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        error.empty() ? "native shadow scene opens" : error.c_str());
  if (!scene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 100}};
  eye.YfovRad = 1;
  eye.ZNearM = .1;
  eye.ZFarM = 20000;
  scene->Eye(eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 16384, true, 0);
  CHECK(scene->Draw(error), error.empty() ? "native source submits" : error.c_str());
  renderer.WaitForGpu();
  std::vector<float> colour;
  std::vector<float> depth;
  std::vector<float> shadow;
  CHECK(renderer.ReadSceneLinear(colour) == ReadState::Ready &&
            renderer.ReadDepth(depth) == ReadState::Ready &&
            renderer.ReadShadowAtlas(shadow) == ReadState::Ready,
        "actual main and shadow outputs are available");
  double radiance = 0;
  for (size_t at = 0; at < colour.size(); ++at) {
    if (at % 4 != 3) { radiance += std::max(0.0f, colour[at]); }
  }
  CHECK(!colour.empty() &&
            std::ranges::all_of(colour, [](float value) { return std::isfinite(value); }) &&
            radiance > 0,
        "the transparent source remains present in the actual main image");
  CHECK(!depth.empty() &&
            std::ranges::any_of(depth, [](float value) { return value > 0; }) == casts,
        "main depth retains only qualified opaque or masked geometry");
  CHECK(!shadow.empty() &&
            std::ranges::any_of(shadow, [](float value) { return value > 0; }) == casts,
        "opaque shadow depth retains real casters without opaque transmission proxies");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native shadow admission");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    CheckCaster(Material{}, true);
    Material masked;
    masked.Alpha = AlphaMode::Masked;
    CheckCaster(masked, true);
    Material blended;
    blended.Alpha = AlphaMode::Blended;
    blended.BaseColour[3] = .5f;
    CheckCaster(blended, false);
    Material refractive;
    refractive.Transmission = .75f;
    refractive.Thickness = 1;
    CheckCaster(refractive, false);
  }
  SDL_Quit();
  return Report();
}
