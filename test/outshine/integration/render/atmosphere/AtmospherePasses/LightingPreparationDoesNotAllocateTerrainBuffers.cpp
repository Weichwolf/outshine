#include "Check.h"
#include "ExecutionPlan.h"
#include "GpuSubmission.h"
#include "IrradianceLayout.h"
#include "Lens.h"
#include "SceneRenderer.h"
#include "Viewing.h"
#include "test/harness/render/NativeTargetFaultDevice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <print>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Test::Gpu::TargetFault fault;
  GpuSubmission submission;
  submission.Context = &fault;
  submission.Create = &Test::Gpu::NativeTargetFaultDevice::Create;
  SceneRenderer renderer(submission);
  PlanSpec spec;
  spec.Outputs = {Resource::EnvironmentSpecular};
  const auto plan = ExecutionPlan::Compile(spec);
  CHECK(plan, "lighting can be prepared without any surface or terrain");
  if (!plan) { return Report(); }
  const auto initialized = renderer.Init({.WidthPx = 16, .HeightPx = 16}, *plan);
  CHECK(initialized, "the actual renderer initializes the lighting-only plan");
  if (!initialized) { return Report(); }
  Viewpoint eye;
  eye.YfovRad = 1;
  constexpr double kNearM = 0.1;
  constexpr double kFarM = 100;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  const auto lens = Lens::From(eye, 16, 16);
  CHECK(lens, "the renderer receives a valid preparation view");
  if (!lens) { return Report(); }
  renderer.SetCamera(CameraBasis{}, *lens);
  const auto allocations = renderer.FrameGraphAllocationCounts();
  std::println(
      "lighting-preparation buffers={} owned={}", fault.CreatedBuffers, allocations.Buffers);
  CHECK(allocations.Buffers == 1 && fault.CreatedBuffers == 1,
        "the requested irradiance is the sole buffer, with no hidden terrain or upload storage");
  const auto completion = renderer.PrepareWorldResources();
  CHECK(completion,
        completion ? "the renderer submits actual native lighting preparation"
                   : completion.error().c_str());
  if (!completion) { return Report(); }
  renderer.WaitForGpu();
  CHECK(renderer.WorldResourcesComplete(*completion), "the lighting publication completed");
  std::array<float, kIrradianceFloats> irradiance{};
  CHECK(renderer.ReadSkyIrradiance(irradiance) == ReadState::Ready &&
            std::ranges::all_of(irradiance, [](float value) { return std::isfinite(value); }),
        "terrain-free lighting produces readable finite irradiance");
  return Report();
}
