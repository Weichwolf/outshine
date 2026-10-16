#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <cstdio>
#include <print>
#include <string>
#include <vector>
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <expected>
#include <chrono>
#include <span>
#include <cstddef>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

auto CompilePlan(std::vector<Resource> outputs, std::vector<Stage> content) {
  PlanSpec spec;
  spec.Outputs = std::move(outputs);
  spec.Content = std::move(content);
  return Compiled::Compile(spec);
}

struct Faults {
  enum class Point { None, Map, Acquire, Submit, FenceWait, Wait };
  Point Next = Point::None;
  size_t Acquired = 0;
  size_t Submitted = 0;
  size_t Cancelled = 0;
  size_t FenceWaits = 0;

  class Observed final : public outshine::Test::Gpu::NativeDeviceProxy {
  public:
    Observed(std::unique_ptr<Native::Device> actual, Faults &faults)
        : NativeDeviceProxy(*actual), Actual_(std::move(actual)), Faults_(faults) {}

    Native::Result<std::span<std::byte>> MapWrite(Native::Buffer &buffer,
                                                  Native::ByteRange range) override {
      if (Faults_.Next == Point::Map) {
        Faults_.Next = Point::None;
        return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                             .Message = "injected initialization map failure"});
      }
      return Backing().MapWrite(buffer, range);
    }

  private:
    std::unique_ptr<Native::Device> Actual_;
    Faults &Faults_;
  };

  GpuSubmission Functions() {
    return {
        .Context = this,
        .Create = [](void *context, Native::DeviceOptions options)
            -> Native::Result<std::unique_ptr<Native::Device>> {
          auto actual = Native::CreateDevice(options);
          if (!actual) { return std::unexpected(actual.error()); }
          return std::make_unique<Observed>(std::move(*actual), *static_cast<Faults *>(context));
        },
        .Acquire = [](void *context,
                      Native::Device *device) -> Native::Result<std::unique_ptr<Native::Commands>> {
          auto &faults = *static_cast<Faults *>(context);
          if (faults.Next == Point::Acquire) {
            faults.Next = Point::None;
            return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                                 .Message = "injected frame acquire failure"});
          }
          ++faults.Acquired;
          return device->Begin();
        },
        .Submit =
            [](void *context,
               Native::Device *device,
               std::unique_ptr<Native::Commands> commands) -> Native::Result<Native::Completion> {
          auto &faults = *static_cast<Faults *>(context);
          ++faults.Submitted;
          if (faults.Next == Point::Submit) {
            faults.Next = Point::None;
            CHECK(commands, "the injected failure receives owned native commands for cancellation");
            ++faults.Cancelled;
            return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                                 .Message = "injected frame submit failure"});
          }
          return device->Submit(std::move(commands));
        },
        .WaitFence =
            [](void *context,
               Native::Device *device,
               Native::Completion completion,
               std::chrono::nanoseconds budget) -> Native::Result<Native::CompletionStatus> {
          auto &faults = *static_cast<Faults *>(context);
          if (faults.Next == Point::Wait) {
            faults.Next = Point::None;
            return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                                 .Message = "injected GPU wait failure"});
          }
          ++faults.FenceWaits;
          if (faults.Next == Point::FenceWait) {
            faults.Next = Point::None;
            return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                                 .Message = "injected frame fence wait failure"});
          }
          return device->Wait(completion, budget);
        }};
  }
};

void InitializationUploads() {
  const auto compiled = CompilePlan({Resource::Surface, Resource::IrradianceBuffer}, {Stage::Sky});
  CHECK(compiled.has_value(),
        "the initialization oracle requires irradiance and its ground bindings");
  if (!compiled) { return; }
  SceneRenderer unopened;
  std::string error;
  CHECK(!unopened.SetGroundClasses({}, {}, error) && !error.empty(),
        "ground upload before renderer initialization reports failure");
  CHECK(!unopened.Settle(error) && !error.empty(), "uninitialized GPU wait is rejected");
  {
    Faults faults;
    SceneRenderer renderer(faults.Functions());
    CHECK(renderer.Init({32, 32}, *compiled).has_value(),
          "renderer initialized for the wait contract");
    if (renderer.DeviceUsable()) {
      faults.Next = Faults::Point::Wait;
      CHECK(!renderer.Settle(error) && error.find("injected GPU wait failure") != std::string::npos,
            "GPU wait failure is returned with an owned native diagnosis");
      SDL_ClearError();
      CHECK(error.find("injected GPU wait failure") != std::string::npos,
            "clearing SDL error does not invalidate the caller's diagnosis");
      CHECK(renderer.Settle(error) && error.empty(), "successful retry clears the old diagnosis");
    }
  }
  for (auto point : {Faults::Point::Map, Faults::Point::Acquire, Faults::Point::Submit}) {
    Faults faults;
    faults.Next = point;
    SceneRenderer renderer(faults.Functions());
    const auto stood = renderer.Init({32, 32}, *compiled);
    CHECK(!stood && stood.error().find("injected") != std::string::npos,
          "failed fallback upload returns the native diagnosis");
    CHECK(!renderer.DeviceUsable(), "failed fallback upload prevents renderer readiness");
  }
}

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
  const auto lastTiming = renderer.LastRenderFrameTiming();
  const auto worstTiming = renderer.SlowestRenderFrameTiming();
  const auto costs = renderer.Costs();
  const auto result = renderer.RenderFrame();
  const char *wanted = point == Faults::Point::Acquire ? "injected frame acquire failure"
                                                       : "injected frame submit failure";
  CHECK(!result && result.error() == wanted, "the original GPU error reaches the frame caller");
  CHECK(faults.Submitted == submits + (point == Faults::Point::Submit ? 1u : 0u),
        "failed acquire never submits; failed submit is attempted exactly once");
  CHECK(renderer.LastRenderFrameTiming().TotalMs == lastTiming.TotalMs &&
            renderer.LastRenderFrameTiming().PhaseMs == lastTiming.PhaseMs &&
            renderer.SlowestRenderFrameTiming().TotalMs == worstTiming.TotalMs &&
            renderer.SlowestRenderFrameTiming().PhaseMs == worstTiming.PhaseMs,
        "rejected GPU commands do not publish successful-frame timings");
  CHECK(renderer.Costs().Frames == costs.Frames &&
            renderer.Costs().HostTotals.TotalMs == costs.HostTotals.TotalMs,
        "rejected commands do not enter successful-frame cost aggregates");
  std::array<float, kIrradianceFloats> irradiance{};
  CHECK(renderer.ReadSkyIrradiance(irradiance) == (cached ? ReadState::Ready : ReadState::Failed),
        "an aborted frame preserves existing LUT validity but cannot publish unsubmitted updates");
}

void ReinitializationInvalidatesFrames(bool temporal) {
  const auto compiled = CompilePlan({Resource::Surface}, {Stage::Sky});
  CHECK(compiled.has_value(), "reinitialization plan compiles");
  if (!compiled) { return; }
  const auto previous =
      temporal
          ? CompilePlan({Resource::Surface}, {Stage::Subjects, Stage::Sky, Stage::TemporalResolve})
          : compiled;
  CHECK(previous.has_value(), "previous render plan compiles");
  if (!previous) { return; }
  Faults faults;
  SceneRenderer renderer(faults.Functions());
  CHECK(renderer.Init({32, 32}, *previous).has_value(), "initial renderer is usable");
  if (!renderer.DeviceUsable()) { return; }
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 1000;
  const auto lens = Lens::From(eye, 32, 32);
  CHECK(lens.has_value(), "reinitialization camera is valid");
  if (!lens) { return; }
  renderer.SetCamera(eye, *lens);
  renderer.SetMedium(kEarthAir);
  renderer.SetSky(
      {.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}, .IlluminanceLux = 10000, .EyeHeightM = 2});
  CHECK(renderer.RenderFrame().has_value() && renderer.Drew(), "old plan has a submitted image");
  faults.Next = Faults::Point::Wait;
  const auto refused = renderer.Init({48, 48}, *compiled);
  CHECK(!refused && refused.error().find("injected GPU wait failure") != std::string::npos,
        "reinitialization returns its failed GPU wait");
  CHECK(renderer.DeviceUsable() && renderer.Drew(),
        "failed GPU wait retains the usable renderer and submitted frame");
  CHECK(renderer.Init({48, 48}, *compiled).has_value(), "replacement plan initializes after retry");
  CHECK(!renderer.Drew(), "reinitialization invalidates previous frame publication");
  std::vector<uint8_t> pixels{1, 2, 3};
  CHECK(renderer.ReadPixels(pixels) == ReadState::Failed &&
            pixels == std::vector<uint8_t>({1, 2, 3}),
        "newly allocated targets cannot be read as a previously submitted image");
  SceneRenderer fresh;
  CHECK(fresh.Init({48, 48}, *compiled).has_value(), "fresh comparison renderer initializes");
  if (!fresh.DeviceUsable() || !renderer.DeviceUsable()) { return; }
  const auto replacementLens = Lens::From(eye, 48, 48);
  CHECK(replacementLens.has_value(), "replacement lens is valid");
  if (!replacementLens) { return; }
  for (auto *target : {&renderer, &fresh}) {
    target->SetCamera(eye, *replacementLens);
    target->SetMedium(kEarthAir);
    target->SetSky(
        {.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}, .IlluminanceLux = 10000, .EyeHeightM = 2});
    CHECK(target->RenderFrame().has_value(), "replacement and fresh renderer submit");
  }
  std::vector<uint8_t> expected;
  CHECK(renderer.ReadPixels(pixels) == ReadState::Ready &&
            fresh.ReadPixels(expected) == ReadState::Ready,
        "both newly submitted targets are readable");
  CHECK(pixels.size() == 48u * 48u * 4u && pixels == expected,
        "reinitialized target matches fresh renderer at new extent");
}

void TransmissionFollowsReplacementPlan() {
  const auto opaque = CompilePlan({Resource::Surface}, {Stage::Subjects, Stage::Sky});
  const auto glass = CompilePlan({Resource::Surface, Resource::SceneComposited},
                                 {Stage::Subjects, Stage::SubjectsTransmissive, Stage::Sky});
  CHECK(opaque.has_value() && glass.has_value(), "transmission switching plans compile");
  if (!opaque || !glass) { return; }
  std::array<SubjectMaterial, 1> materials{};
  materials.front().Row.Transmission = 1;
  SceneRenderer renderer;
  std::string error;
  for (bool enabled : {true, false, true}) {
    const auto initialized = renderer.Init({32, 32}, enabled ? *glass : *opaque);
    if (!initialized) {
      std::println(stderr, "transmission initialization {}: {}", enabled, initialized.error());
    }
    CHECK(initialized.has_value(), "replacement transmission configuration initializes");
    if (!renderer.DeviceUsable()) { return; }
    CHECK(renderer.AppendSubjectMaterials(materials, error) == enabled,
          "material admission follows current transmission plan, not previous plan");
  }
}

void WorldPreparationCompletesLighting() {
  const auto compiled =
      CompilePlan({Resource::Surface, Resource::SceneLinear, Resource::IrradianceBuffer},
                  {Stage::Subjects, Stage::Sky, Stage::TemporalResolve});
  CHECK(compiled.has_value(), "world preparation uses the real atmosphere and temporal plan");
  if (!compiled) { return; }
  Faults faults;
  SceneRenderer actual(faults.Functions()), control;
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 1000;
  const auto lens = Lens::From(eye, 32, 32);
  CHECK(lens.has_value(), "preparation camera is valid");
  if (!lens) { return; }
  for (auto *renderer : {&actual, &control}) {
    const auto started = renderer->Init({32, 32}, *compiled);
    CHECK(started.has_value(), "preparation renderer initializes");
    if (!started) { return; }
    renderer->SetCamera(eye, *lens);
    renderer->SetMedium(kEarthAir);
    renderer->SetSky(
        {.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}, .IlluminanceLux = 10000, .EyeHeightM = 2});
  }
  std::array<float, kIrradianceFloats> prepared{};
  faults.Next = Faults::Point::Submit;
  const auto rejected = actual.PrepareWorldResources();
  CHECK(!rejected && rejected.error() == "injected frame submit failure",
        "failed preparation returns the original submission error");
  CHECK(actual.ReadSkyIrradiance(prepared) == ReadState::Failed,
        "cancelled preparation cannot publish its lighting tables");
  const auto ready = actual.PrepareWorldResources();
  CHECK(ready.has_value(), "world preparation retries the unsubmitted lighting work");
  if (!ready) { return; }
  std::string error;
  CHECK(actual.Settle(error) && actual.WorldResourcesComplete(*ready),
        "world preparation fence covers the lighting resources");
  CHECK(!actual.Drew() && actual.LastRenderFrameTiming().TotalMs == 0,
        "preparation does not render a frame or start its timing history");
  std::vector<uint8_t> pixels;
  CHECK(actual.ReadPixels(pixels) == ReadState::Failed,
        "lighting preparation creates no extra camera image");
  CHECK(actual.ReadSkyIrradiance(prepared) == ReadState::Ready &&
            std::ranges::all_of(prepared, [](float value) { return std::isfinite(value); }) &&
            std::ranges::any_of(prepared, [](float value) { return value > 0; }),
        "finite nonzero sky lighting is readable before the first frame");
  CHECK(control.RenderFrame().has_value(), "the independent renderer executes its first frame");
  const auto expected = Capture(control);
  CHECK(prepared == expected.Irradiance,
        "preloaded irradiance equals the independent frame-generated result");
  CHECK(actual.RenderFrame().has_value(), "prepared renderer executes its first frame");
  Match(expected, Capture(actual));
  for (auto *renderer : {&actual, &control}) {
    renderer->SetSky(
        {.ToSun = {{1, 0, 0}}, .Up = {{0, 1, 0}}, .IlluminanceLux = 10000, .EyeHeightM = 2});
  }
  CHECK(actual.ReadSkyIrradiance(prepared) == ReadState::Failed,
        "a changed sun invalidates the prepared lighting");
  const auto changed = actual.PrepareWorldResources();
  CHECK(changed.has_value(), "changed sunlight is prepared again");
  if (!changed) { return; }
  CHECK(actual.Settle(error) && actual.WorldResourcesComplete(*changed),
        "changed lighting completes before the next frame");
  CHECK(actual.ReadSkyIrradiance(prepared) == ReadState::Ready,
        "changed irradiance is readable after preparation");
  CHECK(control.RenderFrame().has_value(), "independent renderer updates its sunlight");
  const auto changedExpected = Capture(control);
  CHECK(prepared == changedExpected.Irradiance && prepared != expected.Irradiance,
        "prepared sunlight changes and matches the independent renderer");
}

void Exercise() {
  const auto compiled =
      CompilePlan({Resource::Surface, Resource::SceneLinear, Resource::IrradianceBuffer},
                  {Stage::Subjects, Stage::Sky, Stage::TemporalResolve});
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
    const auto started = renderer->Init({32, 32}, *compiled);
    CHECK(started.has_value(), "the real renderer initializes");
    if (!started) {
      std::printf("renderer: %s\n", renderer->WhyNot().c_str());
      return;
    }
    CHECK(!renderer->RenderFrame(), "missing camera cannot become a successful frame");
    renderer->SetCamera(eye, *lens);
    renderer->SetMedium(kEarthAir);
    renderer->SetSky(
        {.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}, .IlluminanceLux = 10000, .EyeHeightM = 2});
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
  CHECK(control.RenderFrame().has_value() && actual.RenderFrame().has_value(),
        "two submitted frames fill the fence ring before its reuse");
  const auto submits = faults.Submitted;
  const auto waits = faults.FenceWaits;
  const auto &costs = actual.Costs();
  CHECK(costs.Frames == 3, "three successful frames are counted once despite rejected attempts");
  double stageTotalMs = 0.0;
  for (const auto &stage : costs.Stages) {
    stageTotalMs += stage.TotalMs;
    CHECK(stage.Frames <= costs.Frames, "stage counts cannot exceed submitted frame count");
  }
  CHECK(stageTotalMs <= costs.HostTotals.PhaseMs[static_cast<size_t>(RenderFramePhase::Encode)],
        "disjoint stage encodes fit inside the host encoding interval across all frames");
  const auto lastTiming = actual.LastRenderFrameTiming();
  const auto worstTiming = actual.SlowestRenderFrameTiming();
  double phaseSumMs = 0;
  for (const double phaseMs : worstTiming.PhaseMs) { phaseSumMs += phaseMs; }
  CHECK(std::abs(phaseSumMs - worstTiming.TotalMs) < 1e-6,
        "slowest frame phases partition that frame rather than unrelated phase maxima");
  CHECK(worstTiming.TotalMs >= lastTiming.TotalMs,
        "the slowest successful frame includes the latest submitted frame");
  faults.Next = Faults::Point::FenceWait;
  const auto fenceRejected = actual.RenderFrame();
  CHECK(!fenceRejected && fenceRejected.error() == "injected frame fence wait failure" &&
            faults.Next == Faults::Point::None,
        "a reused in-flight fence propagates its wait failure");
  CHECK(faults.Submitted == submits && faults.FenceWaits == waits + 1,
        "failed fence waiting prevents acquiring and submitting a new command");
  CHECK(actual.LastRenderFrameTiming().TotalMs == lastTiming.TotalMs &&
            actual.LastRenderFrameTiming().PhaseMs == lastTiming.PhaseMs &&
            actual.SlowestRenderFrameTiming().TotalMs == worstTiming.TotalMs &&
            actual.SlowestRenderFrameTiming().PhaseMs == worstTiming.PhaseMs,
        "failed fence waiting does not publish successful-frame timings");
  CHECK(control.RenderFrame().has_value() && actual.RenderFrame().has_value(),
        "a failed in-flight fence wait preserves the temporal retry");
  Match(Capture(control), Capture(actual));
  control.SetMedium(::outshine::Hazed(::outshine::kEarthAir, 0));
  actual.SetMedium(::outshine::Hazed(::outshine::kEarthAir, 0));
  Reject(actual, faults, Faults::Point::Submit, false);
  CHECK(control.RenderFrame().has_value() && actual.RenderFrame().has_value(),
        "invalidated atmospheric tables are recomputed after failed submission");
  const auto changed = Capture(control);
  Match(changed, Capture(actual));
  CHECK(changed.Irradiance != first.Irradiance,
        "the changed atmosphere is a meaningful LUT update");
  CHECK(faults.Cancelled == 4 && faults.Acquired == faults.Submitted,
        "four rejected submissions release owned commands; rejected fence waiting acquires none");

  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface"};
  declaration.DrawsSky = true;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(actual, declaration, nullptr, scene, error),
        "the runtime scene binds the renderer");
  if (scene) {
    scene->Eye(eye);
    faults.Next = Faults::Point::Acquire;
    CHECK(!scene->Draw(error) && error == "injected frame acquire failure",
          "RuntimeScene::Draw preserves renderer failure instead of reporting false success");
    CHECK(scene->Draw(error), "RuntimeScene::Draw recovers on the next successful GPU frame");
  }
}

void ShadowSubmission() {
  Geometry geometry;
  const int part =
      geometry.addPart("caster", geometry.addSurface("white", Material{}).value()).value();
  CHECK(
      geometry.setPositions(part, std::array<float, 24>{-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
                                                        -1, -1, 1,  1, -1, 1,  1, 1, 1,  -1, 1, 1}),
      "closed caster positions are declared");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 36>{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                                             0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
                                                             0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5}),
        "closed caster faces are declared");
  Faults faults;
  SceneRenderer actual(faults.Functions()), control;
  const std::array renderers{&control, &actual};
  std::array<std::unique_ptr<Core::RuntimeScene>, 2> scenes;
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
  declaration.DrawsSky = true;
  declaration.ShadowRadiusM = 8;
  declaration.KeyLux = 10000;
  declaration.KeyElevationDeg = 45;
  Viewpoint eye;
  eye.EyeM = {{0, 0, 5}};
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  std::string error;
  for (size_t i = 0; i < renderers.size(); ++i) {
    CHECK(Core::RuntimeScene::Open(*renderers[i], declaration, nullptr, scenes[i], error),
          "the shadow scene initializes on the real device");
    if (!scenes[i]) {
      std::printf("shadow setup: %s\n", error.c_str());
      return;
    }
    scenes[i]->Eye(eye);
    renderers[i]->CastsBelow(kNoBatch);
    std::vector<float> untouched{42};
    CHECK(renderers[i]->ReadShadowAtlas(untouched) == ReadState::Failed &&
              untouched == std::vector<float>{42},
          "an unsubmitted atlas is unavailable and leaves the caller's data intact");
  }
  geometry.clear();
  const auto reject = [&] {
    faults.Next = Faults::Point::Submit;
    CHECK(!scenes[1]->Draw(error) && error == "injected frame submit failure",
          "a real shadow recording can fail submission");
    std::vector<float> untouched{42};
    CHECK(actual.ReadShadowAtlas(untouched) == ReadState::Failed &&
              untouched == std::vector<float>{42},
          "cancelled shadow updates cannot be read as current data");
  };
  const auto capture = [](SceneRenderer &renderer) {
    std::vector<float> depth;
    CHECK(renderer.ReadShadowAtlas(depth) == ReadState::Ready, "the submitted atlas is readable");
    CHECK(depth.size() == static_cast<size_t>(kShadowAtlasPx) * kShadowAtlasPx,
          "the complete shadow atlas is compared");
    CHECK(std::ranges::all_of(depth, [](float z) { return std::isfinite(z) && z >= 0 && z <= 1; }),
          "all shadow depths are finite reverse-Z values");
    return depth;
  };
  const auto drawPair = [&] {
    CHECK(scenes[0]->Draw(error) && scenes[1]->Draw(error),
          "both shadow frames submit successfully");
    auto expected = capture(control);
    CHECK(expected == capture(actual),
          "recovered shadow depths exactly match the uninterrupted renderer");
    return expected;
  };
  reject();
  reject();
  const auto first = drawPair();
  CHECK(std::ranges::any_of(first, [](float z) { return z > 0; }),
        "the caster covers real shadow texels");
  CHECK(first == drawPair(), "an unchanged shadow frame retains its submitted depth");
  for (auto *renderer : renderers) { renderer->CastsBelow(0); }
  reject();
  const auto empty = drawPair();
  CHECK(!empty.empty() && std::ranges::all_of(empty, [](float z) { return z == 0; }),
        "changing the caster mask invalidates and clears the old shadow");
  for (auto *renderer : renderers) {
    renderer->CastsBelow(kNoBatch);
    renderer->SetShadowFrame({{0.6f, 0.5f, 0.7f}}, {{0, 1, 0}}, 8);
  }
  reject();
  const auto changed = drawPair();
  CHECK(changed != first && std::ranges::any_of(changed, [](float z) { return z > 0; }),
        "changed light orientation produces a new nonempty shadow after retry");
}

}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"),
        "SDL assertions fail immediately instead of opening a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    InitializationUploads();
    WorldPreparationCompletesLighting();
    Exercise();
    ReinitializationInvalidatesFrames(false);
    ReinitializationInvalidatesFrames(true);
    TransmissionFollowsReplacementPlan();
    ShadowSubmission();
  }
  SDL_Quit();
  Covers("real offscreen acquire/submit faults, LUT publication and retry, temporal pixel parity, "
         "Live error propagation, shadow atlas retry and caster-mask invalidation; swapchain, "
         "pass allocation and readback failure remain separate");
  return Report();
}
