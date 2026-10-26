#include "ImpostorSurface.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr float kNearSample = .25f;
constexpr float kFarSample = .75f;
constexpr double kFarM = 20;
constexpr float kDepthBound = 1e-5f;
constexpr size_t kLeastLayerPixels = 500;
constexpr size_t kFramePixels = size_t{64} * 64;
constexpr std::array<float, 2> kOrthoDepths{16.0f / 19.0f, 12.0f / 19.0f};
constexpr std::array<float, 2> kPerspectiveDepths{4.0f / 19.0f, 1.5f / 19.0f};

void CheckPixels(std::span<const float> depth, std::array<float, 2> expected) {
  size_t near = 0;
  size_t far = 0;
  for (const float value : depth) {
    if (value == 0) { continue; }
    if (std::abs(value - expected[0]) < kDepthBound) {
      ++near;
    } else if (std::abs(value - expected[1]) < kDepthBound) {
      ++far;
    } else {
      CHECK(false, "neither a centre plane nor a bridge enters native depth");
    }
  }
  CHECK(near > kLeastLayerPixels && far > kLeastLayerPixels,
        "both original depths survive camera translation on the GPU");
}

void CheckCamera(Core::RuntimeScene &scene,
                 Render::SceneRenderer &renderer,
                 Render::Viewpoint camera) {
  const bool ortho = camera.Kind == Render::CameraKind::Orthographic;
  for (const double moved : {0.0, 0.5}) {
    camera.EyeM[0] = moved;
    scene.Eye(camera);
    std::string error;
    const bool drawn = scene.Draw(error);
    CHECK(drawn, error.c_str());
    if (!drawn) { return; }
    renderer.WaitForGpu();
    std::vector<float> depth;
    const bool read =
        renderer.ReadDepth(depth) == Render::ReadState::Ready && depth.size() == kFramePixels;
    CHECK(read, "complete native depth reads back");
    if (!read) { return; }
    CheckPixels(depth, ortho ? kOrthoDepths : kPerspectiveDepths);
  }
}

int CheckDepth() {
  const Material material;
  std::vector<ImpostorAtlas::Texel> samples(64);
  for (size_t at = 0; at < samples.size(); ++at) {
    samples[at] = {
        .Normal = {{0, 0, 1}}, .Depth = at % 8 < 4 ? kNearSample : kFarSample, .Surface = 1};
  }
  std::string error;
  const auto atlas = ImpostorAtlas::Create(
      8,
      {},
      2,
      {material},
      {{.TowardEye = {{0, 0, 1}}, .Texels = std::move(samples), .Materials = {}}},
      error);
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
  const bool settled = renderer.Settle(error);
  CHECK(settled, "the offline depth fixture completes its native uploads");
  if (!settled) { return Report(); }
  Render::Viewpoint camera;
  camera.EyeM = {{0, 0, 6}};
  camera.XMagM = camera.YMagM = 3;
  camera.ZNearM = 1;
  camera.ZFarM = kFarM;
  camera.YfovRad = std::numbers::pi / 4;
  for (const auto kind : {Render::CameraKind::Orthographic, Render::CameraKind::Perspective}) {
    camera.Kind = kind;
    CheckCamera(*scene, renderer, camera);
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
