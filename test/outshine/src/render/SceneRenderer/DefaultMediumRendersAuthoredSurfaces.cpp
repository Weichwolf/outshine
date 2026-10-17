#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint32_t kSide = 32;
constexpr size_t kPixels = size_t{kSide} * kSide;
constexpr float kGreen = 0.125f, kBlue = 0.0625f;
constexpr double kNearM = 0.1;
constexpr double kFarM = 10;

void Exercise(bool timing) {
  Geometry geometry;
  Material material;
  material.Unlit = true;
  material.BaseColour = {{1, kGreen, kBlue, 1}};
  const auto surface = geometry.addSurface("authored unlit", material);
  CHECK(surface, "the authored material is valid without a medium declaration");
  if (!surface) { return; }
  const auto part = geometry.addPart("triangle", *surface);
  CHECK(part && geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the authored triangle is complete");
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kSide;
  declaration.Outputs = {"surface"};
  Render::GpuSubmission submission;
  submission.Context = &timing;
  submission.Create = [](void *context, Render::Native::DeviceOptions options) {
    if (!*static_cast<const bool *>(context)) { options.GpuTiming = false; }
    return Render::Native::CreateDevice(options);
  };
  Render::SceneRenderer renderer(submission);
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return; }
  Render::Viewpoint eye;
  eye.EyeM = {{0, 0, 2}};
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  CHECK(scene->Draw(error), error.c_str());
  std::vector<float> linear;
  CHECK(renderer.ReadSceneLinear(linear) == Render::ReadState::Ready &&
            linear.size() == kPixels * 4,
        "the default world produces a complete native scene-linear image");
  if (linear.size() != kPixels * 4) { return; }
  const size_t centre = (size_t{kSide} * (kSide / 2) + kSide / 2) * 4;
  CHECK(std::ranges::all_of(linear, [](float value) { return std::isfinite(value); }),
        "implicit medium preparation never introduces nonfinite authored surface pixels");
  CHECK(linear[centre] == 1 && linear[centre + 1] == kGreen && linear[centre + 2] == kBlue &&
            linear[centre + 3] == 1,
        "default medium preparation preserves the independently authored linear surface colour");
  std::vector<uint8_t> display;
  CHECK(renderer.ReadPixels(display) == Render::ReadState::Ready && display.size() == kPixels * 4,
        "the same submitted native frame reaches the display output");
  const auto first = renderer.Costs();
  CHECK(first.Frames == 1 && first.Gpu.Frames == static_cast<uint64_t>(timing) &&
            first.UnavailableGpuFrames == static_cast<uint64_t>(!timing),
        "each accepted frame contributes exactly one GPU result or an explicit missing value");
  CHECK(std::isfinite(first.Gpu.TotalMs) && first.Gpu.TotalMs == first.Gpu.MostMs &&
            (timing ? first.Gpu.TotalMs > 0 : first.Gpu.TotalMs == 0),
        "completed frames use hardware GPU time and unsupported timing invents no duration");
  CHECK(std::isfinite(first.GpuQueryMs) && first.GpuQueryMs >= 0,
        "GPU result collection records its own CPU cost separately");
  CHECK(renderer.ReadPixels(display) == Render::ReadState::Ready,
        "repeated readback remains valid after the frame timing is retired");
  CHECK(renderer.Costs().Gpu.Frames == first.Gpu.Frames &&
            renderer.Costs().UnavailableGpuFrames == first.UnavailableGpuFrames,
        "repeated readback never duplicates a GPU frame sample or missing value");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    Exercise(true);
    Exercise(false);
  }
  SDL_Quit();
  return Report();
}
