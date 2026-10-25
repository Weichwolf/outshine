#include "FrameTargets.h"
#include "Check.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeTargetTextures.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

struct Counts {
  unsigned Calls = 0, Created = 0, Released = 0, RejectAt = 0;
  bool Empty = false;
};

class Observed final : public Gpu::NativeDeviceProxy {
public:
  Observed(Native::Device &device, Counts &counts) : NativeDeviceProxy(device), Counts_(counts) {}

  Native::Result<std::unique_ptr<Native::Texture>>
  CreateTexture(Native::TextureDescription description, std::string_view label) override {
    if (++Counts_.Calls == Counts_.RejectAt) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::OutOfMemory, .Message = "injected allocation"});
    }
    if (Counts_.Empty) { return std::unique_ptr<Native::Texture>{}; }
    auto texture = Backing().CreateTexture(description, label);
    if (!texture) { return texture; }
    ++Counts_.Created;
    return std::make_unique<Gpu::TargetTexture>(std::move(*texture), Counts_.Released);
  }

private:
  Counts &Counts_;
};

void Exercise(Native::Device &backing) {
  Counts counts;
  Observed device(backing, counts);
  {
    FrameTargets targets;
    PlanSpec spec;
    spec.Content = {Stage::Geometry};
    spec.Outputs = {Resource::SceneHdr, Resource::SceneDepth};
    const auto basic = ExecutionPlan::Compile(spec);
    spec.Content = {Stage::Geometry,
                    Stage::Sky,
                    Stage::TransmissiveGeometry,
                    Stage::CompositeTransmission,
                    Stage::AerialPerspective,
                    Stage::TemporalResolve};
    spec.Outputs = {Resource::SceneHdr,
                    Resource::SceneVelocity,
                    Resource::SceneDepth,
                    Resource::SceneShadingNormal,
                    Resource::SceneSurfaceIdentity,
                    Resource::SceneSurfaceBase,
                    Resource::SceneSurfaceMetalRough,
                    Resource::SceneTransmissive,
                    Resource::SceneComposited,
                    Resource::SceneAerial,
                    Resource::SceneLinear,
                    Resource::FrameTex};
    const auto temporal = ExecutionPlan::Compile(spec);
    CHECK(basic, basic ? "basic allocation contract compiles" : basic.error().c_str());
    CHECK(temporal, temporal ? "temporal allocation contract compiles" : temporal.error().c_str());
    if (!basic || !temporal) { return; }
    CHECK(targets.Replace(device, {.WidthPx = 16, .HeightPx = 8}, **basic),
          "the old attachment set is complete");
    const Native::Texture *const previous = targets.Get(Resource::SceneHdr);
    CHECK(previous && targets.Count() == 2, "the old view retains exactly its requested images");
    if (previous == nullptr) { return; }
    for (const Extent invalid : {Extent{}, Extent{.WidthPx = -1, .HeightPx = 8}}) {
      const auto before = counts.Calls;
      CHECK(!targets.Replace(device, invalid, **temporal) && counts.Calls == before,
            "invalid dimensions fail before touching the GPU");
    }
    constexpr unsigned kFrameImages = 13;
    for (unsigned failure = 1; failure <= kFrameImages; ++failure) {
      counts.RejectAt = counts.Calls + failure;
      const auto replaced = targets.Replace(device, {.WidthPx = 32, .HeightPx = 16}, **temporal);
      CHECK(!replaced && replaced.error().Kind == Native::ErrorKind::OutOfMemory,
            "every candidate allocation can fail explicitly");
      CHECK(targets.Get(Resource::SceneHdr) == previous && targets.Count() == 2 &&
                counts.Created - counts.Released == 2,
            "failure destroys partial candidates and preserves every old attachment");
    }
    counts.RejectAt = 0;
    counts.Empty = true;
    CHECK(!targets.Replace(device, {.WidthPx = 32, .HeightPx = 16}, **temporal) &&
              targets.Get(Resource::SceneHdr) == previous && counts.Created - counts.Released == 2,
          "a device returning an empty success cannot publish a missing attachment");
    counts.Empty = false;
    CHECK(targets.Replace(device, {.WidthPx = 32, .HeightPx = 16}, **temporal),
          "a complete candidate can replace the old view after failure");
    CHECK(targets.Count() == kFrameImages && counts.Created - counts.Released == kFrameImages &&
              targets.Get(Resource::SceneLinear, 0) && targets.Get(Resource::SceneLinear, 1) &&
              targets.Get(Resource::SceneLinear, 0) != targets.Get(Resource::SceneLinear, 1),
          "the complete requested set retains distinct history images and all material buffers");
  }
  CHECK(counts.Created == counts.Released, "the frame owner releases every owned image");
}
}

int main() {
  const auto device = Native::CreateDevice();
  CHECK(device, "fault checks use the real native allocator behind the proxy");
  if (device) { Exercise(**device); }
  return Report();
}
