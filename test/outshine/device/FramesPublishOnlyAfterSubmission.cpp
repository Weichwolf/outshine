#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "Check.h"
#include "Live.h"
#include "SceneRenderer.h"

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

struct Faults {
  enum class Point { None, Acquire, Submit };
  Point Next = Point::None;
  size_t Acquired = 0;
  size_t Submitted = 0;
  size_t Cancelled = 0;

  GpuSubmission Functions() {
    return {.Context = this,
            .Acquire = [](void *context, SDL_GPUDevice *device) -> SDL_GPUCommandBuffer * {
              auto &faults = *static_cast<Faults *>(context);
              if (faults.Next == Point::Acquire) {
                faults.Next = Point::None;
                SDL_SetError("injected frame acquire failure");
                return nullptr;
              }
              ++faults.Acquired;
              return SDL_AcquireGPUCommandBuffer(device);
            },
            .Submit = [](void *context, SDL_GPUCommandBuffer *commands) -> SDL_GPUFence * {
              auto &faults = *static_cast<Faults *>(context);
              ++faults.Submitted;
              if (faults.Next == Point::Submit) {
                faults.Next = Point::None;
                // Only offscreen commands are cancelled: cancellation after swapchain acquire is
                // illegal.
                CHECK(SDL_CancelGPUCommandBuffer(commands),
                      "the injected failure consumes real commands");
                ++faults.Cancelled;
                SDL_SetError("injected frame submit failure");
                return nullptr;
              }
              return SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
            }};
  }
};

struct Snapshot {
  std::vector<float> Linear;
  std::array<float, kIrradianceFloats> Irradiance{};
};

Snapshot Capture(SceneRenderer &renderer) {
  Snapshot image;
  CHECK(renderer.ReadSceneLinear(image.Linear) == ReadState::Ready,
        "the submitted temporal image is readable");
  CHECK(image.Linear.size() == 32u * 32u * 4u, "the complete image is read");
  float brightest = 0;
  for (size_t at = 0; at < image.Linear.size(); ++at) {
    if (at % 4u != 3u) { brightest = std::max(brightest, image.Linear[at]); }
  }
  CHECK(std::isfinite(brightest) && brightest > 0,
        "the daylight sky writes nonzero finite RGB, not only alpha");
  std::printf("daylight maximum scene-linear RGB: %.9g\n", static_cast<double>(brightest));
  CHECK(renderer.ReadSkyIrradiance(image.Irradiance) == ReadState::Ready,
        "submitted sky irradiance is readable");
  CHECK(std::ranges::all_of(image.Irradiance, [](float value) { return std::isfinite(value); }) &&
            std::ranges::any_of(image.Irradiance, [](float value) { return value > 0; }),
        "the atmospheric kernels produced finite nonzero irradiance");
  return image;
}

void Match(const Snapshot &expected, const Snapshot &actual) {
  size_t different = 0, nonfinite = 0;
  double maximum = 0;
  for (size_t at = 0; at < std::min(expected.Linear.size(), actual.Linear.size()); ++at) {
    const double left = expected.Linear[at], right = actual.Linear[at];
    if (!std::isfinite(left) || !std::isfinite(right)) { ++nonfinite; }
    if (left == right) { continue; }
    if (different < 4) {
      std::printf("linear[%zu]: control %.9g recovered %.9g\n", at, left, right);
    }
    ++different;
    if (std::isfinite(left) && std::isfinite(right)) {
      maximum = std::max(maximum, std::abs(left - right));
    }
  }
  std::printf(
      "temporal comparison: %zu channels differ, %zu nonfinite pairs, max finite delta %.9g\n",
      different,
      nonfinite,
      maximum);
  CHECK(nonfinite == 0, "both complete images contain finite linear radiance");
  CHECK(expected.Linear == actual.Linear,
        "recovered temporal pixels equal the renderer with only successful frames");
  CHECK(expected.Irradiance == actual.Irradiance,
        "recovered LUT values equal the uninterrupted renderer");
}

void Reject(SceneRenderer &renderer, Faults &faults, Faults::Point point, bool cached) {
  faults.Next = point;
  const auto submits = faults.Submitted;
  const auto result = renderer.RenderFrame();
  const char *wanted = point == Faults::Point::Acquire ? "injected frame acquire failure"
                                                       : "injected frame submit failure";
  CHECK(!result && result.error() == wanted, "the original GPU error reaches the frame caller");
  CHECK(faults.Submitted == submits + (point == Faults::Point::Submit ? 1u : 0u),
        "failed acquire never submits; failed submit is attempted exactly once");
  std::array<float, kIrradianceFloats> irradiance{};
  CHECK(renderer.ReadSkyIrradiance(irradiance) == (cached ? ReadState::Ready : ReadState::Failed),
        "an aborted frame preserves existing LUT validity but cannot publish unsubmitted updates");
}

void Exercise() {
  const auto compiled = Compiled::Compile(
      {.Outputs = {Resource::Surface, Resource::SceneLinear, Resource::IrradianceBuffer},
       .Content = {Stage::Subjects, Stage::Sky, Stage::TemporalResolve}});
  CHECK(compiled.has_value(), "the real sky, irradiance and temporal plan compiles");
  if (!compiled) { return; }
  CHECK((*compiled)->Holds(Stage::TemporalResolve) && (*compiled)->Holds(Stage::MediumRadiance),
        "the oracle exercises temporal history and the atmosphere stages");
  Faults faults;
  SceneRenderer actual(faults.Functions());
  SceneRenderer control;
  CHECK(!actual.RenderFrame(), "an uninitialized renderer reports failure");
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 1000;
  const auto lens = Lens::From(eye, 32, 32);
  CHECK(lens.has_value(), "the camera projection is valid");
  if (!lens) { return; }
  for (auto *renderer : {&control, &actual}) {
    renderer->Init({32, 32}, *compiled);
    CHECK(renderer->DeviceUsable(), "the real renderer initializes");
    if (!renderer->DeviceUsable()) {
      std::printf("renderer: %s\n", renderer->WhyNot().c_str());
      return;
    }
    CHECK(!renderer->RenderFrame(), "missing camera cannot become a successful frame");
    renderer->SetCamera(eye, *lens);
    renderer->SetMedium(kEarthAir);
    renderer->SetSky({{0, 1, 0}}, {{0, 1, 0}}, 10000, 2);
  }
  for (int attempt = 0; attempt < 2; ++attempt) {
    Reject(actual, faults, Faults::Point::Acquire, false);
    Reject(actual, faults, Faults::Point::Submit, false);
    CHECK(!actual.Drew(), "failed first frames do not establish previous-frame state");
  }
  CHECK(control.RenderFrame().has_value() && actual.RenderFrame().has_value(),
        "the first successful frame recovers after repeated real command cancellation");
  const auto first = Capture(control);
  Match(first, Capture(actual));
  Reject(actual, faults, Faults::Point::Submit, true);
  CHECK(control.RenderFrame().has_value() && actual.RenderFrame().has_value(),
        "a warm temporal frame also recovers");
  Match(Capture(control), Capture(actual));
  control.SetMedium(Hazed(kEarthAir, 0));
  actual.SetMedium(Hazed(kEarthAir, 0));
  Reject(actual, faults, Faults::Point::Submit, false);
  CHECK(control.RenderFrame().has_value() && actual.RenderFrame().has_value(),
        "invalidated atmospheric tables are recomputed after failed submission");
  const auto changed = Capture(control);
  Match(changed, Capture(actual));
  CHECK(changed.Irradiance != first.Irradiance,
        "the changed atmosphere is a meaningful LUT update");
  CHECK(faults.Cancelled == 4 && faults.Acquired == faults.Submitted,
        "each acquired offscreen command is consumed once, including four rejected submissions");

  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface"};
  declaration.DrawsSky = true;
  std::unique_ptr<Core::Live> live;
  std::string error;
  CHECK(Core::Live::Open(actual, declaration, nullptr, live, error),
        "the live API binds the renderer");
  if (live) {
    live->Eye(eye);
    faults.Next = Faults::Point::Acquire;
    CHECK(!live->Draw(error) && error == "injected frame acquire failure",
          "Live::Draw preserves renderer failure instead of reporting false success");
    CHECK(live->Draw(error), "Live::Draw recovers on the next successful GPU frame");
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  Covers("real offscreen acquire/submit faults, LUT publication and retry, temporal pixel parity, "
         "Live error propagation; swapchain, pass allocation and readback failure remain separate");
  return Report();
}
