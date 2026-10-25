#include "Atmosphere.h"
#include "Check.h"
#include "ExecutionPlan.h"
#include "GpuSubmission.h"
#include "Lens.h"
#include "SceneRenderer.h"
#include "Viewing.h"
#include <SDL3/SDL.h>
#include <cstddef>
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

constexpr int kSide = 64;
constexpr size_t kImageValues = size_t{kSide} * kSide * 4;
constexpr double kNearM = 0.1, kFarM = 1000, kIlluminanceLux = 10000;

std::vector<float> RenderSequence(bool reject) {
  struct Injection {
    bool Refuse = false;
    unsigned Refused = 0;
  } injection;

  GpuSubmission submission;
  submission.Context = &injection;
  submission.Submit =
      [](void *context,
         Native::Device *device,
         std::unique_ptr<Native::Commands> commands) -> Native::Result<Native::Completion> {
    auto &state = *static_cast<Injection *>(context);
    if (std::exchange(state.Refuse, false)) {
      ++state.Refused;
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Driver, .Message = "rejected test frame"});
    }
    return device->Submit(std::move(commands));
  };
  SceneRenderer renderer(submission);
  PlanSpec spec;
  spec.Outputs = {Resource::SceneLinear, Resource::Surface};
  spec.Content = {Stage::Geometry, Stage::Sky, Stage::TemporalResolve};
  const auto plan = ExecutionPlan::Compile(spec);
  CHECK(plan, "actual temporal frame plan compiles");
  if (!plan) { return {}; }
  const auto initialized = renderer.Init({.WidthPx = kSide, .HeightPx = kSide}, *plan);
  CHECK(initialized, "native temporal attachments initialize");
  if (!initialized) { return {}; }
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  const auto lens = Lens::From(eye, kSide, kSide);
  CHECK(lens, "test view is representable");
  if (!lens) { return {}; }
  CameraBasis camera;
  renderer.SetCamera(camera, *lens);
  renderer.SetMedium(kEarthAir);
  renderer.SetSky({.ToSun = {{0, 1, 0}},
                   .Up = {{0, 1, 0}},
                   .IlluminanceLux = kIlluminanceLux,
                   .EyeHeightM = 2});
  std::string error;
  CHECK(renderer.Settle(error), "initial GPU uploads complete");
  for (int frame = 0; frame < 3; ++frame) {
    camera.EyeM[0] = frame;
    renderer.SetCamera(camera, *lens);
    if (reject && frame == 1) {
      const auto previous = renderer.LastSubmittedCamera();
      injection.Refuse = true;
      CHECK(!renderer.RenderFrame() && injection.Refused == 1,
            "encoded native frame submission is refused");
      CHECK(renderer.LastSubmittedCamera().Serial == previous.Serial &&
                renderer.LastSubmittedCamera().Basis.EyeM == previous.Basis.EyeM,
            "rejected frame preserves the accepted camera publication");
    }
    CHECK(renderer.RenderFrame(), "the next requested frame submits successfully");
  }
  std::vector<float> image;
  CHECK(renderer.ReadSceneLinear(image) == ReadState::Ready, "submitted temporal HDR reads back");
  return image;
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native temporal integration starts video");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    const auto control = RenderSequence(false);
    const auto retried = RenderSequence(true);
    CHECK(control.size() == kImageValues && control == retried,
          "a rejected submission and retry produce the unchanged temporal image");
    if (!control.empty()) {
      CHECK(control[0] > 0 || control[1] > 0 || control[2] > 0,
            "the compared HDR contains sky radiance rather than only clear alpha");
    }
  }
  SDL_Quit();
  return Report();
}
