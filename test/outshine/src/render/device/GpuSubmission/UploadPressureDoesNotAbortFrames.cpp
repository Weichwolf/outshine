#include "Check.h"
#include "Atmosphere.h"
#include "ExecutionPlan.h"
#include "Lens.h"
#include "Viewing.h"
#include "SceneRenderer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <ratio>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kNearM = 0.1, kFarM = 1000, kSkyLux = 10000;
constexpr int kSide = 32;

struct Pressure {
  unsigned Busy = 0, Begins = 0, Waits = 0;
  bool ForeverBusy = false, AcquireFailure = false, WaitFailure = false, Pending = false;
  std::chrono::nanoseconds Budget{}, Waited{};
};

class Observed final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  Observed(std::unique_ptr<Native::Device> actual, Pressure &pressure)
      : NativeDeviceProxy(*actual), Actual_(std::move(actual)), Pressure_(pressure) {}

  Native::Result<std::unique_ptr<Native::Commands>> Begin() override {
    ++Pressure_.Begins;
    if (Pressure_.AcquireFailure) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Driver, .Message = "owned acquire failure"});
    }
    if (Pressure_.ForeverBusy || Pressure_.Busy != 0) {
      if (Pressure_.Busy != 0) { --Pressure_.Busy; }
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Busy, .Message = "owned queue pressure"});
    }
    return Backing().Begin();
  }

  Native::Result<Native::CompletionStatus> Wait(Native::Completion completion,
                                                std::chrono::nanoseconds budget) override {
    ++Pressure_.Waits;
    Pressure_.Budget = budget;
    const auto began = std::chrono::steady_clock::now();
    auto ready = [&] -> Native::Result<Native::CompletionStatus> {
      if (Pressure_.WaitFailure) {
        return std::unexpected(
            Native::Error{.Kind = Native::ErrorKind::Driver, .Message = "owned wait failure"});
      }
      if (Pressure_.Pending) { return Native::CompletionStatus::Pending; }
      return Backing().Wait(completion, budget);
    }();
    Pressure_.Waited = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - began);
    return ready;
  }

private:
  std::unique_ptr<Native::Device> Actual_;
  Pressure &Pressure_;
};

enum class Case { Temporary, Permanent, Driver, Wait, Pending };

void Exercise(Case mode) {
  Pressure pressure;
  const GpuSubmission functions{.Context = &pressure,
                                .Create = [](void *context, Native::DeviceOptions options)
                                    -> Native::Result<std::unique_ptr<Native::Device>> {
                                  auto actual = Native::CreateDevice(options);
                                  if (!actual) { return std::unexpected(actual.error()); }
                                  return std::make_unique<Observed>(
                                      std::move(*actual), *static_cast<Pressure *>(context));
                                }};
  SceneRenderer renderer(functions);
  PlanSpec spec;
  spec.Outputs = {Resource::Surface};
  spec.Content = {Stage::Geometry, Stage::Sky};
  const auto plan = ExecutionPlan::Compile(spec);
  CHECK(plan.has_value(), "pressure fixture compiles the actual frame pipeline");
  if (!plan) { return; }
  const auto initialized = renderer.Init({.WidthPx = kSide, .HeightPx = kSide}, *plan);
  CHECK(initialized.has_value(), "pressure fixture initializes native rendering");
  if (!initialized) { return; }
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  const auto lens = Lens::From(eye, kSide, kSide);
  CHECK(lens.has_value(), "pressure fixture has a valid camera");
  if (!lens) { return; }
  CameraBasis camera;
  renderer.SetCamera(camera, *lens);
  renderer.SetMedium(kEarthAir);
  renderer.SetSky(
      {.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}, .IlluminanceLux = kSkyLux, .EyeHeightM = 2});
  std::string error;
  CHECK(renderer.Settle(error) && renderer.RenderFrame().has_value(),
        "fixture owns an accepted upload/frame completion before pressure");
  const auto previous = renderer.LastSubmittedCamera();
  if (previous.Serial == 0) { return; }
  camera.EyeM[0] = 1;
  renderer.SetCamera(camera, *lens);
  pressure.Busy = mode == Case::Driver ? 0 : 1;
  pressure.ForeverBusy = mode == Case::Permanent;
  pressure.AcquireFailure = mode == Case::Driver;
  pressure.WaitFailure = mode == Case::Wait;
  pressure.Pending = mode == Case::Pending;
  const auto begins = pressure.Begins;
  const auto waits = pressure.Waits;
  const auto frame = renderer.RenderFrame();
  if (mode == Case::Temporary) {
    CHECK(frame.has_value() && renderer.LastSubmittedCamera().Serial > previous.Serial &&
              renderer.LastSubmittedCamera().Basis.EyeM == camera.EyeM,
          "temporary upload pressure delivers the requested frame rather than aborting");
    CHECK(pressure.Begins == begins + 2 && pressure.Waits == waits + 1 &&
              pressure.Budget == std::chrono::seconds(5),
          "frame acquisition performs one bounded wait and one retry");
    const double waitedMs = std::chrono::duration<double, std::milli>(pressure.Waited).count();
    CHECK(renderer.LastRenderFrameTiming()
                  .PhaseMs[static_cast<size_t>(RenderFramePhase::FenceWait)] >= waitedMs,
          "actual upload queue waiting belongs to host fence time");
  } else {
    CHECK(!frame && renderer.LastSubmittedCamera().Serial == previous.Serial &&
              renderer.LastSubmittedCamera().Basis.EyeM == previous.Basis.EyeM,
          "failed pressure recovery cannot publish a future camera");
    CHECK(pressure.Begins == begins + (mode == Case::Permanent ? 2 : 1) &&
              pressure.Waits == waits + (mode == Case::Driver ? 0 : 1),
          "driver, pending, failed wait and permanent Busy never cause an unbounded retry");
  }
  pressure.Busy = 0;
  pressure.ForeverBusy = pressure.AcquireFailure = pressure.WaitFailure = pressure.Pending = false;
}
}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"), "assertions fail without a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native pressure fixture starts video");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    for (const auto mode :
         {Case::Temporary, Case::Permanent, Case::Driver, Case::Wait, Case::Pending}) {
      Exercise(mode);
    }
  }
  SDL_Quit();
  return Report();
}
