#include "CloudViewProbe.h"
#include "NativeGpuCheck.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>

namespace outshine::Test::CloudViewProbe {
using namespace outshine::Test::Gpu;

std::unique_ptr<GraphicsPipeline>
MakePipeline(Device &device, const char *vertex, const char *fragment, bool scene) {
  const auto vertexCode = Spirv(vertex);
  const auto fragmentCode = Spirv(fragment);
  std::array<ColourTarget, 4> targets;
  for (auto &target : targets) { target.Texels = Format::Rgba32Float; }
  auto pipeline = Require(device.CreateGraphicsPipeline(
      {.Vertex = {.Spirv = vertexCode},
       .Fragment = ShaderCode{.Spirv = fragmentCode},
       .Streams = {},
       .Attributes = {},
       .Colours = std::span(targets).first(scene ? 1 : targets.size()),
       .Depth = scene ? std::optional{DepthTarget{}} : std::nullopt,
       .Culling = Cull::None,
       .Label = "CloudFrontRasterControl"}));
  if (!scene) {
    size_t medium = 0;
    size_t weather = 0;
    for (const auto &binding : pipeline->Layout().Bindings) {
      const auto &resource = binding.Resource;
      if (resource.Set == 3 && resource.Slot == 2 && resource.Kind == BindingKind::Uniform) {
        CHECK(resource.MinimumBytes == sizeof(outshine::Render::MediumViewParameters),
              "the actual fragment3/2 frame stays224 bytes");
        ++medium;
      }
      if (resource.Set == 3 && resource.Slot == 3 && resource.Kind == BindingKind::Uniform) {
        CHECK(resource.MinimumBytes == sizeof(WeatherFrame),
              "the private fragment3/3 weather payload is96 bytes");
        ++weather;
      }
    }
    CHECK(medium == 1 && weather == 1, "both independently owned snapshots are reflected once");
  }
  return pipeline;
}

}
