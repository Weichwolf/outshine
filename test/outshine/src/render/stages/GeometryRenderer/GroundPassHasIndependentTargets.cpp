#include "GeometryRenderer.h"
#include "RenderScene.h"
#include "Gpu.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class Observed final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  std::vector<size_t> GroundTargets;
  size_t Computes = 0;

  Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
  CreateGraphicsPipeline(const Native::GraphicsDescription &description) override {
    if (!description.Colours.empty() && (description.Label.starts_with("groundLattice") ||
                                         description.Label.starts_with("groundPatchLattice"))) {
      GroundTargets.push_back(description.Colours.size());
    }
    return Backing().CreateGraphicsPipeline(description);
  }

  Native::Result<std::unique_ptr<Native::ComputePipeline>>
  CreateComputePipeline(std::span<const uint32_t> source, Native::ComputeOptions options) override {
    ++Computes;
    return Backing().CreateComputePipeline(source, options);
  }
};

void Probe(Observed &device) {
  outshine::Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 4;
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "opaque geometry owns its HDR target");
  RenderScene storage;
  GeometryRenderer geometry(storage);
  GeometryPipelineBinding binding;
  Require(geometry.Configure(binding, gpu, nullptr, nullptr, false));
  geometry.UsePipelines(binding);
  CHECK(device.GroundTargets.empty(),
        "opaque configuration does not build unreachable terrain colour pipelines");
  Require(binding.ConfigureGroundSelection(device));
  const auto selectionComputes = device.Computes;
  CHECK(gpu.SceneColours.Add(Resource::SceneShadingNormal), "the terrain pass adds its own output");
  Require(geometry.ConfigureGround(binding, gpu));
  CHECK(
      device.GroundTargets == std::vector<size_t>({2, 2}),
      "lattice and patch terrain pipelines use the ground pass targets rather than opaque targets");
  Require(binding.ConfigureGroundSelection(device));
  CHECK(device.Computes == selectionComputes,
        "late terrain colour configuration preserves previously compiled patch selection");
  gpu.SceneColours = {};
  for (const auto output : {Resource::SceneHdr,
                            Resource::SceneShadingNormal,
                            Resource::SceneSurfaceIdentity,
                            Resource::SceneSurfaceBase,
                            Resource::SceneSurfaceMetalRough}) {
    CHECK(gpu.SceneColours.Add(output), "terrain material capture has its own declared attachment");
  }
  Require(geometry.ConfigureGround(binding, gpu));
  CHECK(geometry.Ground().CapturesMaterial() && device.GroundTargets.back() == 5,
        "terrain resource bindings derive from its own capture mode independently of opaque mode");
}
}

int main() {
  auto backend = Require(Native::CreateDevice({.Validation = true}));
  Observed device(*backend);
  Probe(device);
  return outshine::Test::Report();
}
