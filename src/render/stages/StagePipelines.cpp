#include "StagePipelines.h"
#include "SpirvFile.h"
#include <array>
#include <utility>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

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
  const std::array colours{Native::ColourTarget{.Texels = target, .Rgb = {}, .Alpha = {}}};
  return CreateFullscreenPipeline(device, fragmentPath, colours);
}

namespace {
struct ShaderFiles {
  std::string_view Vertex, Fragment;
};

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CompileFullscreen(Native::Device &device,
                  ShaderFiles files,
                  std::span<const Native::ColourTarget> colours,
                  std::optional<Native::DepthTarget> depth) {
  const auto vertex = ReadSpirv(files.Vertex);
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vertex.error()});
  }
  const auto fragment = ReadSpirv(files.Fragment);
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = fragment.error()});
  }
  return device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                        .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                        .Streams = {},
                                        .Attributes = {},
                                        .Colours = colours,
                                        .Depth = depth,
                                        .Culling = Native::Cull::None,
                                        .Label = files.Fragment});
}

}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreateFullscreenPipeline(Native::Device &device,
                         std::string_view fragmentPath,
                         std::span<const Native::ColourTarget> colours,
                         std::optional<Native::DepthTarget> depth) {
  return CompileFullscreen(
      device,
      {.Vertex = "build/shaders/fullscreen.vert.spv", .Fragment = fragmentPath},
      colours,
      depth);
}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreateFullscreenNdcPipeline(Native::Device &device,
                            std::string_view fragmentPath,
                            std::span<const Native::ColourTarget> colours,
                            std::optional<Native::DepthTarget> depth) {
  return CompileFullscreen(
      device,
      {.Vertex = "build/shaders/fullscreenNdc.vert.spv", .Fragment = fragmentPath},
      colours,
      depth);
}
}
