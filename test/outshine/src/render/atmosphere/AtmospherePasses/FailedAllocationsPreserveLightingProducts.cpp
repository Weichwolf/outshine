#include "AtmospherePasses.h"
#include "Check.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeTargetFaultDevice.h"
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

class EmptyTextureDevice final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;

  Native::Result<std::unique_ptr<Native::Texture>>
  CreateTexture([[maybe_unused]] Native::TextureDescription description,
                [[maybe_unused]] std::string_view label) override {
    return std::unique_ptr<Native::Texture>{};
  }
};

void Exercise() {
  auto backing = Native::CreateDevice();
  CHECK(backing, "lighting products allocate without a window or world");
  if (!backing) { return; }
  outshine::Test::Gpu::TargetFault fault;
  outshine::Test::Gpu::NativeTargetFaultDevice device(std::move(*backing), fault);
  AtmospherePasses products;
  CHECK(products.Allocations().Textures == 0 && products.Allocations().Buffers == 0 &&
            products.Allocations().Samplers == 0,
        "a product owner allocates nothing until requested");
  PlanSpec spec;
  spec.Outputs = {Resource::EnvironmentSpecular};
  const auto plan = ExecutionPlan::Compile(spec);
  CHECK(plan, "lighting-only dependencies compile without a surface");
  if (!plan) { return; }
  for (const auto &resource : kResources) {
    if (!AtmospherePasses::Owns(resource.Id) || !(**plan).Holds(resource.Id)) { continue; }
    CHECK(products.Create(device, **plan, resource.Id), "requested lighting product allocates");
  }
  constexpr size_t kLookups = 4;
  CHECK(products.Allocations().Textures == kLookups && products.Allocations().Buffers == 1 &&
            products.Allocations().Samplers == 1,
        "lighting owns four fixed lookups, irradiance and one sampler");
  for (const auto &resource : kResources) {
    if (const auto *previous = products.Texture(resource.Id)) {
      fault.Arm(outshine::Test::Gpu::TargetFailure::Texture);
      const auto failed = products.Create(device, **plan, resource.Id);
      CHECK(!failed && products.Texture(resource.Id) == previous,
            "texture allocation failure preserves the previous published product");
      EmptyTextureDevice empty(device);
      CHECK(!products.Create(empty, **plan, resource.Id) &&
                products.Texture(resource.Id) == previous,
            "empty native success cannot replace a valid lookup");
    }
  }
  const auto *irradiance = products.Irradiance();
  const auto *sampler = products.Sampler();
  fault.Arm(outshine::Test::Gpu::TargetFailure::Buffer);
  CHECK(!products.Create(device, **plan, Resource::IrradianceBuffer) &&
            products.Irradiance() == irradiance,
        "failed buffer replacement preserves irradiance");
  fault.Arm(outshine::Test::Gpu::TargetFailure::Sampler);
  CHECK(!products.Create(device, **plan, Resource::LutSampler) && products.Sampler() == sampler,
        "failed sampler replacement preserves all existing pass bindings");
  const auto textures = fault.CreatedTextures;
  const auto buffers = fault.CreatedBuffers;
  CHECK(!products.Create(device, **plan, Resource::SceneHdr) && !products.Has(Resource::SceneHdr) &&
            fault.CreatedTextures == textures && fault.CreatedBuffers == buffers,
        "surface attachments are rejected before native allocation");
}
}

int main() {
  Exercise();
  return outshine::Test::Report();
}
