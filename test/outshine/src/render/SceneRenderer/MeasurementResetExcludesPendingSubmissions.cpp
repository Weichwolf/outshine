#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kNanosecondsPerMs = 1'000'000;

struct Timings {
  bool Hold = false, Fail = false;
  unsigned Waits = 0;
  std::vector<Native::Completion> Submitted, Queried;
};

class TimingDevice final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  TimingDevice(std::unique_ptr<Native::Device> device, Timings &state)
      : NativeDeviceProxy(*device), Device_(std::move(device)), State_(state) {}

  static Native::Result<std::unique_ptr<Native::Device>> Create(void *context,
                                                                Native::DeviceOptions options) {
    auto device = Native::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<TimingDevice>(std::move(*device), *static_cast<Timings *>(context));
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    auto submitted = Backing().Submit(std::move(commands));
    if (submitted) { State_.Submitted.push_back(*submitted); }
    return submitted;
  }

  Native::Result<Native::CompletionStatus> Poll(Native::Completion completion) override {
    return State_.Hold ? Native::Result<Native::CompletionStatus>{Native::CompletionStatus::Pending}
                       : Backing().Poll(completion);
  }

  Native::Result<Native::CompletionStatus> Wait(Native::Completion completion,
                                                std::chrono::nanoseconds budget) override {
    ++State_.Waits;
    State_.Hold = false;
    return Backing().Wait(completion, budget);
  }

  Native::Result<Native::SubmissionGpuTiming>
  SubmissionGpuTime(Native::Completion completion) override {
    State_.Queried.push_back(completion);
    if (State_.Fail) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Unsupported,
                                           .Message = "injected unavailable timing"});
    }
    auto result = Backing().SubmissionGpuTime(completion);
    if (result) { result->ElapsedNanoseconds = completion.Serial * kNanosecondsPerMs; }
    return result;
  }

private:
  std::unique_ptr<Native::Device> Device_;
  Timings &State_;
};

void Exercise() {
  Geometry geometry;
  const auto part = geometry.addPart("triangle", MaterialInstance{});
  CHECK(part && geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "measurement fixture has visible geometry");
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface"};
  Timings state;
  SceneRenderer renderer({.Context = &state, .Create = TimingDevice::Create});
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return; }
  auto eye = *Viewpoint::LookAt({.EyeM = {{0, 0, 4}}, .AimM = {{0, 0, 0}}}, 0.0);
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  scene->Eye(eye);
  state.Hold = true;
  CHECK(scene->Draw(error), "an earlier frame is submitted without completing its GPU sample");
  const auto camera = renderer.LastSubmittedCamera();
  const auto waits = state.Waits;
  CHECK(renderer.Costs().Frames == 1 && renderer.Costs().Gpu.Frames == 0,
        "the previous frame remains genuinely pending before reset");
  renderer.ResetMeasurements();
  CHECK(state.Waits == waits && renderer.Costs().Frames == 0 && renderer.Costs().Gpu.Frames == 0,
        "reset clears the interval without a GPU wait or hidden frame");
  CHECK(renderer.LastSubmittedCamera().Serial == camera.Serial && renderer.Drew(),
        "reset preserves the submitted camera and successfully rendered image");
  renderer.WaitForGpu();
  CHECK(state.Queried.empty() && renderer.Costs().Gpu.Frames == 0 &&
            renderer.Costs().UnavailableGpuFrames == 0,
        "late completion of the prior frame never contaminates the new interval");
  std::vector<uint8_t> before, after;
  CHECK(renderer.ReadPixels(before) == ReadState::Ready && !before.empty(),
        "the previous image remains readable after reset");
  renderer.ResetMeasurements();
  CHECK(renderer.ReadPixels(after) == ReadState::Ready && before == after,
        "resetting measurements changes neither pixels nor static scene state");
  const bool first = scene->Draw(error);
  CHECK(first, "the first new measured frame submits");
  if (!first) { return; }
  const auto firstSubmission = state.Submitted.back();
  const bool second = scene->Draw(error);
  CHECK(second, "the second new measured frame submits");
  if (!second) { return; }
  const auto secondSubmission = state.Submitted.back();
  renderer.WaitForGpu();
  const auto costs = renderer.Costs();
  CHECK(state.Queried.size() == 2 && costs.Frames == 2 && costs.Gpu.Frames == 2,
        "only the two new CPU and GPU frames belong to this interval");
  const auto expected = firstSubmission.Serial + secondSubmission.Serial;
  CHECK(costs.Gpu.TotalMs == expected && costs.Gpu.MostMs == secondSubmission.Serial &&
            costs.UnavailableGpuFrames == 0,
        "GPU sum and maximum use only the new submission identities");
  renderer.ResetMeasurements();
  renderer.ResetMeasurements();
  CHECK(renderer.Costs().Frames == 0 && renderer.SlowestRenderFrameTiming().TotalMs == 0,
        "repeated reset starts an empty interval including its CPU maximum");
  state.Fail = true;
  CHECK(scene->Draw(error), "an unavailable timing does not refuse an otherwise valid frame");
  renderer.WaitForGpu();
  CHECK(renderer.Costs().Frames == 1 && renderer.Costs().Gpu.Frames == 0 &&
            renderer.Costs().UnavailableGpuFrames == 1,
        "a new unavailable timing is counted once in its own interval");
  state.Fail = false;
  renderer.ResetMeasurements();
  CHECK(scene->Draw(error), "a new measurement interval recovers from unavailable timing");
  renderer.WaitForGpu();
  CHECK(renderer.Costs().Frames == 1 && renderer.Costs().Gpu.Frames == 1 &&
            renderer.Costs().UnavailableGpuFrames == 0,
        "the prior unavailable sample does not persist into a fresh interval");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
