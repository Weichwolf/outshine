#include "Check.h"
#include "FragmentArms.h"
#include "GeometryRenderer.h"
#include "Gpu.h"
#include "SurfaceState.h"
#include "VertexArms.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <print>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;

class TransmissionDevice final : public Test::Gpu::NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;

  bool RejectUnreachable = false;
  uint32_t Created = 0;
  uint32_t Rejected = 0;

  Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
  CreateGraphicsPipeline(const Native::GraphicsDescription &description) override {
    if (RejectUnreachable &&
        std::ranges::find(TransmissionLabels(), description.Label) == TransmissionLabels().end()) {
      ++Rejected;
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "opaque pipeline requested by glass pass"});
    }
    auto made = Backing().CreateGraphicsPipeline(description);
    if (made) { ++Created; }
    return made;
  }

private:
  static const std::vector<std::string_view> &TransmissionLabels() {
    static const auto labels = [] {
      std::vector<std::string_view> result;
      for (const auto &row : kVertexLayouts) {
        for (const auto kind : {SurfaceKind::ThinTransmissive, SurfaceKind::Refractive}) {
          result.emplace_back(
              GeometryRenderer::FragmentEntry(SurfaceDomain::Subject, kind, row.Layout));
        }
      }
      return result;
    }();
    return labels;
  }
};

uint32_t RequiredPipelines(bool transmission) {
  uint32_t count = 0;
  for (const auto domain : {SurfaceDomain::Subject, SurfaceDomain::Ground}) {
    for (const auto &row : kVertexLayouts) {
      for (const auto kind : {SurfaceKind::Opaque,
                              SurfaceKind::Masked,
                              SurfaceKind::Blended,
                              SurfaceKind::ThinTransmissive,
                              SurfaceKind::Refractive}) {
        const bool transmits =
            kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive;
        if (transmission == transmits &&
            DomainPresents(domain, ShadingArmOf(row.Layout), CarriesUv(row.Layout), kind)) {
          count += 2;
        }
      }
    }
  }
  return count;
}

void CheckGlass(TransmissionDevice &device, bool velocity, bool rejectUnreachable) {
  Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 2;
  CHECK(gpu.SceneColours.Add(Resource::SceneTransmissive), "glass colour target is declared");
  if (velocity) {
    CHECK(gpu.SceneColours.Add(Resource::SceneVelocity), "glass velocity target is declared");
  }
  const auto behind =
      Test::Gpu::Require(device.CreateTexture({.Width = 2,
                                               .Height = 2,
                                               .Texels = Native::Format::Rgba16Float,
                                               .Use = Native::TextureUse::Sampled}));
  const auto sampler = Test::Gpu::Require(device.CreateSampler({}));
  GeometryPipelineBinding binding;
  GeometryRenderer draw;
  device.Created = device.Rejected = 0;
  device.RejectUnreachable = rejectUnreachable;
  const auto result = draw.Configure(binding, gpu, behind.get(), sampler.get(), false);
  CHECK(result, "the glass pass configures with only its reachable native pipelines");
  if (!result) { std::println(stderr, "{}", result.error().Message); }
  CHECK(device.Rejected == 0, "glass never requests an opaque fragment pipeline");
  CHECK(device.Created == RequiredPipelines(true),
        "native pipeline creation matches only the transmissive shader/layout states");
  if (result) {
    draw.UsePipelines(binding);
    CHECK(draw.PipelineCount() == device.Created, "the pass owns exactly the states it created");
  }
  std::println("glass velocity={} guarded={} created={} required={}",
               velocity,
               rejectUnreachable,
               device.Created,
               RequiredPipelines(true));
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    auto native = Test::Gpu::Require(Native::CreateDevice({.Validation = true}));
    TransmissionDevice device(*native);
    CHECK(RequiredPipelines(false) > 0 && RequiredPipelines(true) > 0,
          "both material passes have supported shader states");
    for (const bool velocity : {false, true}) {
      for (const bool guarded : {false, true}) { CheckGlass(device, velocity, guarded); }
    }
  }
  SDL_Quit();
  return Test::Report();
}
