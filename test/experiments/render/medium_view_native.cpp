#include "MediumViewProbe.h"
#include <cstdint>
#include <cstddef>
#include <optional>
#include "NativeGpuCheck.h"
#include "Check.h"
#include <memory>
#include <span>
#include <array>
#include <print>
#include <string>

namespace {
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using namespace outshine::Test::MediumViewProbe;

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertexPath, const char *fragmentPath, uint32_t count) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  std::array<ColourTarget, 3> targets;
  for (auto &target : targets) { target.Texels = Format::Rgba32Float; }
  auto pipeline = Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                         .Fragment = ShaderCode{.Spirv = fragment},
                                                         .Streams = {},
                                                         .Attributes = {},
                                                         .Colours = std::span(targets).first(count),
                                                         .Depth = std::nullopt,
                                                         .Culling = Cull::None,
                                                         .Label = "MediumViewControl"}));
  const auto &bindings = pipeline->Layout().Bindings;
  size_t frames = 0;
  for (const auto &binding : bindings) {
    const auto &resource = binding.Resource;
    if (resource.Set == 3 && resource.Slot == 2 && resource.Kind == BindingKind::Uniform) {
      CHECK(resource.MinimumBytes == sizeof(MediumViewParameters),
            "actual native reflection preserves MediumView224 at fragment3/2");
      ++frames;
    }
  }
  CHECK(frames == 1, "the native pipeline consumes exactly one shared medium frame");
  return pipeline;
}
}

int main(int argc, char **argv) {
  if (argc != 6) {
    std::println(stderr,
                 "usage: medium-view-native geometry.vert geometry.frag fullscreen.vert "
                 "opaque.frag output-prefix");
    return 2;
  }
  auto device = Require(CreateDevice({.Validation = true}));
  std::println("device={} geometry_frames={} opaque_frames={}",
               device->Limits().Name,
               GeometryScenarios().size(),
               2 * Scenarios().size() + 3);
  const auto geometry = Pipeline(*device, argv[1], argv[2], 3);
  const auto opaque = Pipeline(*device, argv[3], argv[4], 1);
  const std::string prefix = argv[5];
  Geometry(*device, *geometry, (prefix + ".geometry").c_str());
  Opaque(*device, *opaque, (prefix + ".opaque").c_str());
  return Report();
}
