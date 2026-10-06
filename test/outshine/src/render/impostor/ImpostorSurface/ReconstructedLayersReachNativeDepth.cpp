#include "ImpostorSurface.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace {
int CheckDepth() {
  using namespace outshine;
  using namespace outshine::Test;
  Material material;
  std::vector<Content::ImpostorAtlas::Texel> samples(64);
  for (size_t at = 0; at < samples.size(); ++at) {
    samples[at] = {.Normal = {{0, 0, 1}}, .Depth = at % 8 < 4 ? 0.25f : 0.75f, .Surface = 1};
  }
  std::string error;
  const auto atlas = Content::ImpostorAtlas::Create(
      8, {}, 2, {material}, {{.TowardEye = {{0, 0, 1}}, .Texels = std::move(samples)}}, error);
  CHECK(atlas.has_value(), error.c_str());
  if (!atlas) { return Report(); }
  const auto surface = Render::BuildImpostorSurface(*atlas, 0);
  CHECK(surface && surface->wellFormed(), "depth patches form a native material-bearing surface");
  if (!surface) { return Report(); }
  Core::Declaration declaration;
  declaration.InitialGeometry = &*surface;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Outputs = {"sceneDepth"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return Report(); }
  Render::Viewpoint camera;
  camera.EyeM = {{0, 0, 6}};
  camera.Kind = Render::CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = 3;
  camera.ZNearM = 1;
  camera.ZFarM = 20;
  camera.YfovRad = std::numbers::pi / 4;
  for (auto kind : {Render::CameraKind::Orthographic, Render::CameraKind::Perspective}) {
    camera.Kind = kind;
    const bool ortho = kind == Render::CameraKind::Orthographic;
    const float nearDepth = ortho ? 16.0f / 19.0f : 4.0f / 19.0f;
    const float farDepth = ortho ? 12.0f / 19.0f : 1.5f / 19.0f;
    for (double moved : {0.0, 0.5}) {
      camera.EyeM[0] = moved;
      scene->Eye(camera);
      CHECK(scene->Draw(error), error.c_str());
      renderer.WaitForGpu();
      std::vector<float> depth;
      CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready, "native depth reads back");
      size_t near = 0, far = 0;
      for (float value : depth) {
        if (value == 0) { continue; }
        if (std::abs(value - nearDepth) < 1e-5f) {
          ++near;
        } else if (std::abs(value - farDepth) < 1e-5f) {
          ++far;
        } else {
          CHECK(false, "neither a centre plane nor a bridge enters native depth");
        }
      }
      CHECK(near > 500 && far > 500, "both original depths survive camera translation on the GPU");
    }
  }
  return Report();
}
}

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    CHECK(false, SDL_GetError());
    return outshine::Test::Report();
  }
  const int result = CheckDepth();
  SDL_Quit();
  return result;
}
