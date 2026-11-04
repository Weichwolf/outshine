#include "AtmospherePasses.h"
#include "ExecutionPlan.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <memory>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  using namespace outshine::Test::Gpu;
  auto device = Require(Native::CreateDevice({.Validation = true}));
  PlanSpec specification;
  specification.Outputs = {Resource::EnvironmentSpecular};
  const auto plan = ExecutionPlan::Compile(specification);
  CHECK(plan, "the environment layout has a valid native plan");
  if (!plan) { return Report(); }
  std::shared_ptr<Native::Texture> image;
  std::shared_ptr<Native::Sampler> sampler;
  {
    AtmospherePasses owner;
    Require(owner.Create(*device, **plan, Resource::EnvironmentSpecular));
    Require(owner.Create(*device, **plan, Resource::LutSampler));
    image = owner.TextureLease(Resource::EnvironmentSpecular);
    sampler = owner.SamplerLease();
    CHECK(image && sampler && owner.Texture(Resource::EnvironmentSpecular) == image.get(),
          "borrowed bindings and held field leases name the same native resources");
    CHECK(!owner.TextureLease(Resource::IrradianceBuffer),
          "non-image resources cannot produce a texture lease");
    Require(owner.Create(*device, **plan, Resource::EnvironmentSpecular));
    Require(owner.Create(*device, **plan, Resource::LutSampler));
    CHECK(owner.Texture(Resource::EnvironmentSpecular) != image.get() &&
              owner.Sampler() != sampler.get(),
          "new frame allocations replace the owner's bindings without overwriting readers");
  }
  const std::weak_ptr<Native::Texture> oldImage = image;
  const std::weak_ptr<Native::Sampler> oldSampler = sampler;
  CHECK(!oldImage.expired() && !oldSampler.expired() && image->Description().Width > 0,
        "a retired world owner cannot invalidate the resources held by a field submission");
  image.reset();
  sampler.reset();
  CHECK(oldImage.expired() && oldSampler.expired(),
        "retired image and sampler ownership end with their final field lease");
  return Report();
}
