#include "ImpostorSurface.h"
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {
void Exercise(size_t firstMip) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(64);
  view.Texels[27] = {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1};
  Material material;
  material.BaseColour = {{.8f, .4f, .2f, 1}};
  std::string error;
  const auto atlas = ImpostorAtlas::Create(8, {}, 1, {material}, {view}, error);
  CHECK(atlas.has_value(), "a sparse original surface defines fractional area");
  if (!atlas) { return; }
  const auto card = BuildFractionalImpostorSurface(*atlas, 0, firstMip);
  CHECK(card.has_value(), "the native optical card is available");
  if (!card) { return; }
  Core::Declaration declaration;
  declaration.InitialGeometry = &*card;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear", "sceneDepth", "shadowAtlas"};
  declaration.KeyLux = 100;
  declaration.KeyBearingDeg = 180;
  declaration.ShadowRadiusM = 16;
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        error.empty() ? "fractional native world opens" : error.c_str());
  if (!scene) { return; }
  Viewpoint eye;
  eye.Kind = CameraKind::Orthographic;
  eye.EyeM = {{0, 0, 10}};
  eye.XMagM = eye.YMagM = 16;
  eye.ZNearM = .1;
  eye.ZFarM = 100;
  scene->Eye(eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 16, false, 0);
  CHECK(scene->Draw(error), error.empty() ? "fractional native world submits" : error.c_str());
  renderer.WaitForGpu();
  std::vector<float> colour;
  std::vector<float> depth;
  std::vector<float> shadow;
  CHECK(renderer.ReadSceneLinear(colour) == ReadState::Ready &&
            colour.size() == size_t{32} * 32 * 4 && renderer.ReadDepth(depth) == ReadState::Ready &&
            renderer.ReadShadowAtlas(shadow) == ReadState::Ready,
        "main radiance, main depth and the actual shadow atlas are readable");
  double radiance = 0;
  double coverage = 0;
  for (size_t at = 0; at < colour.size(); ++at) {
    if (at % 4 == 3) {
      coverage += colour[at];
    } else {
      radiance += colour[at];
    }
  }
  CHECK(std::ranges::all_of(colour, [](float value) { return std::isfinite(value); }) &&
            radiance > 0 && coverage > 0 && coverage < 1,
        "minified subpixel source coverage remains finite, lit and fractional");
  CHECK(!depth.empty() && std::ranges::all_of(depth, [](float value) { return value == 0; }),
        "the closed optical support never becomes an opaque main-depth surface");
  CHECK(!shadow.empty() && std::ranges::all_of(shadow, [](float value) { return value == 0; }) &&
            renderer.ShadowCastCount() == 0,
        "unqualified optical depth never becomes an opaque shadow caster");
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for actual fractional impostors");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    for (size_t mip = 0; mip <= 3; ++mip) { Exercise(mip); }
  }
  SDL_Quit();
  return Report();
}
