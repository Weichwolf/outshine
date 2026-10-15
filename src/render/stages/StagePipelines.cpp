#include "StagePipelines.h"
#include "SpirvFile.h"
#include <array>
#include <utility>

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace outshine::Render {

namespace {

bool Matches(const Native::ComputeLayout &layout, const ComputeShape &shape) {
  const std::array expectedGroups{shape.GroupX, shape.GroupY, shape.GroupZ};
  if (layout.LocalSize != expectedGroups) { return false; }
  std::array<uint32_t, 6> counts{};
  for (const auto &binding : layout.Bindings) {
    switch (binding.Kind) {
      case Native::BindingKind::SampledTexture: ++counts[0]; break;
      case Native::BindingKind::StorageTexture:
        ++counts[binding.Usage == Native::Access::Read ? 1 : 2];
        break;
      case Native::BindingKind::StorageBuffer:
        ++counts[binding.Usage == Native::Access::Read ? 3 : 4];
        break;
      case Native::BindingKind::Uniform: ++counts[5]; break;
    }
  }
  return counts == std::array{shape.Samplers,
                              shape.ReadOnlyTextures,
                              shape.ReadWriteTextures,
                              shape.ReadOnlyBuffers,
                              shape.ReadWriteBuffers,
                              shape.UniformBuffers};
}

}

Native::Result<std::unique_ptr<Native::ComputePipeline>>
CompileComputePipeline(Native::Device &device, const ComputeShaderDescriptor &shader) {
  const auto words = ReadSpirv(shader.Path);
  if (!words) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = words.error()});
  }
  auto pipeline = device.CreateComputePipeline(*words, {.Label = shader.Path});
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  if (!Matches((*pipeline)->Layout(), shader.Shape)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = std::string(shader.Path) +
                                 " does not match its declared compute bindings or workgroup"});
  }
  return std::move(*pipeline);
}

Native::Result<std::unique_ptr<Native::ComputePipeline>>
CreateComputePipeline(Native::Device &device, ComputeShaderId shader) {
  const auto *const descriptor = FindComputeShader(shader);
  if (descriptor == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "unknown built-in compute shader ID"});
  }
  return CompileComputePipeline(device, *descriptor);
}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreatePresentPipeline(Native::Device &device, Native::Format target) {
  const char *fragmentPath = nullptr;
  switch (target) {
    case Native::Format::Bgra8Srgb:
    case Native::Format::Rgba8Srgb: fragmentPath = "build/shaders/present.frag.spv"; break;
    case Native::Format::Bgra8Unorm:
    case Native::Format::Rgba8Unorm: fragmentPath = "build/shaders/presentUnorm.frag.spv"; break;
    default:
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Unsupported,
                        .Message = "presentation requires an SDR colour target"});
  }
  const auto vertex = ReadSpirv("build/shaders/fullscreen.vert.spv");
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vertex.error()});
  }
  const auto fragment = ReadSpirv(fragmentPath);
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = fragment.error()});
  }
  const std::array colours{Native::ColourTarget{.Texels = target, .Rgb = {}, .Alpha = {}}};
  return device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                        .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                        .Streams = {},
                                        .Attributes = {},
                                        .Colours = colours,
                                        .Depth = {},
                                        .Culling = Native::Cull::None,
                                        .Label = fragmentPath});
}

}
