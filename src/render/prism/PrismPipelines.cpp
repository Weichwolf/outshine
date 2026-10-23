#include "PrismPipelines.h"
#include "SpirvFile.h"
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Render {
namespace {
struct PipelineSources {
  std::string_view Vertex;
  std::string_view Fragment;
};

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreatePrismPipeline(Native::Device &device,
                    PipelineSources sources,
                    std::span<const Native::ColourTarget> targets) {
  auto vertex = ReadSpirv(sources.Vertex);
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(vertex.error())});
  }
  auto fragment = ReadSpirv(sources.Fragment);
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(fragment.error())});
  }
  return device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                        .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                        .Streams = {},
                                        .Attributes = {},
                                        .Colours = targets,
                                        .Depth = Native::DepthTarget{},
                                        .Culling = Native::Cull::Front,
                                        .ClampDepth = true,
                                        .Label = "prism grid"});
}
}

Native::Result<void>
PrismPipelines::ConfigureColour(Native::Device &device,
                                const SurfaceOutputs &outputs,
                                std::span<const Native::ColourTarget> targets) {
  auto pipeline = CreatePrismPipeline(device,
                                      {.Vertex = "build/shaders/prism-colour.vert.spv",
                                       .Fragment = outputs.FragmentPath("prismLit")},
                                      targets);
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  Colour_ = std::move(*pipeline);
  return {};
}

Native::Result<void> PrismPipelines::ConfigureDepth(Native::Device &device) {
  auto pipeline = CreatePrismPipeline(device,
                                      {.Vertex = "build/shaders/prism-depth.vert.spv",
                                       .Fragment = "build/shaders/prismDepth.frag.spv"},
                                      {});
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  Depth_ = std::move(*pipeline);
  return {};
}
}
