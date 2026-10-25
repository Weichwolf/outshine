#include "AtmospherePasses.h"
#include "Check.h"
#include "StageSubmission.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr float kInitialSunZenith = 0.5f;
constexpr double kGroundAlbedo = 0.2;
constexpr double kGroundAlbedoChange = 0.5;
constexpr std::array kStages{Stage::MediumTransmittance,
                             Stage::MediumMultiScatter,
                             Stage::MediumRadiance,
                             Stage::Irradiance,
                             Stage::EnvironmentSpecular};

class CountedCommands final : public Test::Gpu::NativeCommandsProxy {
public:
  using NativeCommandsProxy::NativeCommandsProxy;
  size_t Dispatches = 0;

  Native::Result<void> Dispatch(const Native::ComputePipeline &pipeline,
                                std::span<const Native::Binding> bindings,
                                Native::Workgroups groups) override {
    ++Dispatches;
    return NativeCommandsProxy::Dispatch(pipeline, bindings, groups);
  }
};

size_t Record(AtmospherePasses &products, Native::Device &device, bool submit) {
  auto commands = device.Begin();
  CHECK(commands, "lighting command recording starts");
  if (!commands) { return 0; }
  CountedCommands counted(std::move(*commands));
  StageSubmission pending;
  const PassRecording recording{
      .Commands = &counted, .Submission = pending, .Area = {}, .Clip = {}};
  for (const auto stage : kStages) {
    CHECK(products.Encode(stage, recording), "configured native lighting pass records");
  }
  if (submit) {
    const auto completion = device.Submit(counted.Take());
    CHECK(completion, "native lighting submission succeeds");
    if (completion) {
      pending.Commit();
      const auto ready = device.Wait(*completion, std::chrono::seconds(5));
      CHECK(ready && *ready == Native::CompletionStatus::Ready,
            "lighting resources remain alive until native work finishes");
    }
  }
  return counted.Dispatches;
}

void Exercise() {
  auto device = Native::CreateDevice();
  CHECK(device, "lighting compute needs neither scene geometry nor a window");
  if (!device) { return; }
  PlanSpec spec;
  spec.Outputs = {Resource::EnvironmentSpecular};
  const auto plan = ExecutionPlan::Compile(spec);
  CHECK(plan, "lighting preparation has a standalone dependency plan");
  if (!plan) { return; }
  AtmospherePasses products;
  for (const auto &resource : kResources) {
    if (!AtmospherePasses::Owns(resource.Id) || !(**plan).Holds(resource.Id)) { continue; }
    CHECK(products.Create(**device, **plan, resource.Id), "requested native product exists");
  }
  const Render::Gpu gpu{.Device = device->get(), .SceneColours = {}};
  for (const auto stage : kStages) {
    CHECK(products.Configure(stage, gpu), "lighting pipeline owns its actual resource bindings");
  }
  AtmosphereDeclaration declaration{
      .Air = kEarthAir,
      .CosSunZenith = kInitialSunZenith,
      .EyeHeightM = 2,
      .GroundAlbedo = {{kGroundAlbedo, kGroundAlbedo, kGroundAlbedo}}};
  products.Declare(declaration);
  CHECK(Record(products, **device, false) == kStages.size(),
        "first recording prepares all products");
  for (const auto stage : kStages) {
    CHECK(!products.Settled(stage), "discarded commands cannot publish a cached lighting product");
  }
  CHECK(Record(products, **device, true) == kStages.size(),
        "cancelled recording retries every unsubmitted product");
  for (const auto stage : kStages) {
    CHECK(products.Settled(stage), "native commit publishes lighting");
  }
  products.Declare(declaration);
  CHECK(Record(products, **device, true) == 0, "unchanged physical inputs repeat no lighting work");
  declaration.EyeHeightM += 1;
  products.Declare(declaration);
  CHECK(Record(products, **device, true) == 2,
        "height affects view radiance and reflection, not medium or diffuse irradiance");
  declaration.GroundAlbedo[0] += kGroundAlbedoChange;
  products.Declare(declaration);
  CHECK(Record(products, **device, true) == 1,
        "surface albedo only changes environment reflection");
  declaration.CosSunZenith = 1;
  products.Declare(declaration);
  CHECK(Record(products, **device, true) == 3,
        "solar direction changes view, irradiance and environment products");
  declaration.Air.MieScatteringPerKm *= 2;
  products.Declare(declaration);
  CHECK(Record(products, **device, true) == kStages.size(),
        "changed air invalidates every dependent lighting product");
}
}

int main() {
  Exercise();
  return outshine::Test::Report();
}
