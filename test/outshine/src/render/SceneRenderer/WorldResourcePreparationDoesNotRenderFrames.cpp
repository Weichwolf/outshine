#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "SubjectTypes.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

struct Faults {
  enum class Failure { None, Acquire, Submit };
  Failure Next = Failure::None;
  bool Hold = false;
  unsigned Waits = 0;

  GpuSubmission Functions() {
    return {
        .Context = this,
        .Acquire = [](void *context, SDL_GPUDevice *device) -> SDL_GPUCommandBuffer * {
          auto &faults = *static_cast<Faults *>(context);
          if (faults.Next == Failure::Acquire) {
            faults.Next = Failure::None;
            SDL_SetError("injected preparation acquire failure");
            return nullptr;
          }
          return SDL_AcquireGPUCommandBuffer(device);
        },
        .Submit = [](void *context, SDL_GPUCommandBuffer *commands) -> SDL_GPUFence * {
          auto &faults = *static_cast<Faults *>(context);
          if (faults.Next == Failure::Submit) {
            faults.Next = Failure::None;
            CHECK(SDL_CancelGPUCommandBuffer(commands), "failed submission consumes commands");
            SDL_SetError("injected preparation submit failure");
            return nullptr;
          }
          return SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        },
        .WaitFence =
            [](void *context, SDL_GPUDevice *device, SDL_GPUFence *const *fences, uint32_t count) {
              ++static_cast<Faults *>(context)->Waits;
              return SDL_WaitForGPUFences(device, true, fences, count);
            },
        .QueryFence =
            [](void *context, SDL_GPUDevice *device, SDL_GPUFence *fence) {
              return !static_cast<Faults *>(context)->Hold && SDL_QueryGPUFence(device, fence);
            },
        .WaitIdle =
            [](void *context, SDL_GPUDevice *device) {
              ++static_cast<Faults *>(context)->Waits;
              return SDL_WaitForGPUIdle(device);
            }};
  }
};

void Exercise() {
  Geometry geometry;
  const auto material = geometry.addSurface("wall", Material{});
  CHECK(material.has_value(), "native material exists");
  if (!material) { return; }
  const auto part = geometry.addPart("caster", *material);
  constexpr std::array<float, 24> positions{-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
                                            -1, -1, 1,  1, -1, 1,  1, 1, 1,  -1, 1, 1};
  constexpr std::array<uint32_t, 36> indices{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                                             3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
  CHECK(part && geometry.setPositions(*part, positions) && geometry.setTriangles(*part, indices),
        "native closed caster is complete");
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "sceneDepth", "sceneVelocity", "shadowAtlas"};
  declaration.DrawsSky = true;
  declaration.ShadowRadiusM = 8;
  declaration.KeyLux = 10000;
  declaration.KeyElevationDeg = 45;
  Faults faults;
  SceneRenderer prepared(faults.Functions());
  SceneRenderer control;
  std::unique_ptr<Core::RuntimeScene> preparedScene;
  std::unique_ptr<Core::RuntimeScene> controlScene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(prepared, declaration, nullptr, preparedScene, error) &&
            Core::RuntimeScene::Open(control, declaration, nullptr, controlScene, error),
        "prepared and control scenes open with identical geometry");
  if (!preparedScene || !controlScene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  preparedScene->Eye(eye);
  controlScene->Eye(eye);
  faults.Waits = 0;
  for (const auto failure : {Faults::Failure::Acquire, Faults::Failure::Submit}) {
    faults.Next = failure;
    const auto refused = prepared.PrepareWorldResources();
    CHECK(!refused && refused.error().find("injected preparation") != std::string::npos,
          "preparation failure returns its owned diagnosis and allows retry");
    CHECK(!prepared.Drew() && prepared.LastSubmittedCamera().Serial == 0,
          "failed preparation never publishes a camera frame");
  }
  const std::array<SubjectMaterial, 1> rebuiltMaterials{};
  CHECK(prepared.SetSubjectMaterials(rebuiltMaterials, error) &&
            control.SetSubjectMaterials(rebuiltMaterials, error),
        "both scenes receive the final native materials after retrying acquisition");
  auto uploads = prepared.PrepareWorldResources();
  CHECK(uploads.has_value(), "upload preparation succeeds after both failures");
  if (!uploads) { return; }
  faults.Hold = true;
  const auto transferBytes = prepared.PieceAllocations().TransferBytes;
  CHECK(transferBytes > 0, "world preparation retains its upload storage until completion");
  CHECK(!prepared.WorldResourcesComplete(*uploads) && faults.Waits == 0,
        "pending upload completion is polled without a blocking GPU wait");
  CHECK(prepared.PieceAllocations().TransferBytes == transferBytes,
        "an incomplete fence retains upload storage");
  faults.Hold = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!prepared.WorldResourcesComplete(*uploads) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(prepared.WorldResourcesComplete(*uploads),
        "real GPU uploads complete within the test bound");
  CHECK(prepared.PieceAllocations().TransferBytes == 0,
        "completed world uploads release scratch while retaining resident geometry");
  CHECK(!prepared.Drew() && prepared.LastSubmittedCamera().Serial == 0 && faults.Waits == 0,
        "successful preparation does not draw, advance the frame ring or publish a camera");
  std::vector<float> preparedShadow;
  CHECK(prepared.ReadShadowAtlas(preparedShadow) == ReadState::Ready &&
            std::ranges::any_of(preparedShadow, [](float depth) { return depth > 0; }),
        "the static shadow product is complete before any scene frame");
  CHECK(preparedScene->Draw(error) && controlScene->Draw(error), "both first frames render");
  CHECK(prepared.ShadowCastCount() == 0 && control.ShadowCastCount() > 0,
        "the prepared scene reuses complete shadows while the cold control actually casts");
  std::vector<float> actual;
  std::vector<float> expected;
  CHECK(prepared.ReadDepth(actual) == ReadState::Ready &&
            control.ReadDepth(expected) == ReadState::Ready,
        "both first-frame depth targets are readable");
  CHECK(actual.size() == 32u * 32u && actual == expected &&
            std::ranges::any_of(actual, [](float depth) { return depth > 0; }),
        "prepared uploads preserve the control's first visible geometry exactly");
  std::vector<float> firstShadow;
  std::vector<float> controlShadow;
  CHECK(prepared.ReadShadowAtlas(firstShadow) == ReadState::Ready &&
            control.ReadShadowAtlas(controlShadow) == ReadState::Ready &&
            firstShadow == preparedShadow && firstShadow == controlShadow,
        "preloaded shadows exactly match first-frame shadow generation and remain intact");
  CHECK(prepared.LastSubmittedCamera().Serial == 1 && control.LastSubmittedCamera().Serial == 1,
        "only the explicit draws count as frames");
}

}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
