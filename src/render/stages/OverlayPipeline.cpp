#include "OverlayDraw.h"
#include "SpirvFile.h"
#include "math/Vec2.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<void>
OverlayPipeline::Configure(const Gpu &gpu, Native::Sampler *smooth, Native::Format targetFormat) {
  if (gpu.Device == nullptr || smooth == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "overlay pipeline requires native device and sampler"});
  }
  auto vertex = ReadSpirv("build/shaders/overlay.vert.spv");
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vertex.error()});
  }
  auto fragment = ReadSpirv("build/shaders/overlay.frag.spv");
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = fragment.error()});
  }
  const std::array streams{Native::VertexStream{
      .Slot = 0, .Stride = sizeof(OverlayQuad), .Rate = Native::VertexRate::Instance}};
  constexpr size_t attributesCount = 5;
  std::array<Native::VertexAttribute, attributesCount> attributes;
  for (uint32_t i = 0; i < attributes.size(); ++i) {
    attributes[i] = {.Location = i,
                     .Stream = 0,
                     .Offset = i * 4u * static_cast<uint32_t>(sizeof(float)),
                     .Format = Native::VertexFormat::Float4};
  }
  const std::array targets{
      Native::ColourTarget{.Texels = targetFormat,
                           .Blend = true,
                           .Rgb = {.Source = Native::BlendFactor::One,
                                   .Destination = Native::BlendFactor::OneMinusSourceAlpha},
                           .Alpha = {.Source = Native::BlendFactor::One,
                                     .Destination = Native::BlendFactor::OneMinusSourceAlpha}}};
  auto made =
      gpu.Device->CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                          .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                          .Streams = streams,
                                          .Attributes = attributes,
                                          .Colours = targets,
                                          .Depth = {},
                                          .Culling = Native::Cull::None,
                                          .Label = "overlay"});
  if (!made) { return std::unexpected(made.error()); }
  Pipe = std::move(*made);
  Smooth = smooth;
  Encodes = targetFormat == Native::Format::Rgba8Srgb || targetFormat == Native::Format::Bgra8Srgb;
  return {};
}

Native::Result<void> OverlayPipeline::Encode(const OverlayDraw &overlay,
                                             [[maybe_unused]] const FrameContext &ctx,
                                             const PassRecording &into) {
  if (overlay.Held() == 0) { return {}; }
  if (!Pipe || overlay.Vertices() == nullptr || overlay.Atlas() == nullptr || WidthPx <= 0 ||
      HeightPx <= 0 || into.Commands == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "overlay draw is missing native resources or target extent"});
  }

  struct Frame {
    Vec2f TargetPx;
    float EncodesSrgb;
    float Pad;
  };

  static_assert(sizeof(Frame) == 16 && offsetof(Frame, EncodesSrgb) == sizeof(Vec2f));
  const Frame frame{.TargetPx = {{static_cast<float>(WidthPx), static_cast<float>(HeightPx)}},
                    .EncodesSrgb = Encodes ? 1.0f : 0.0f,
                    .Pad = 0};
  const std::array bindings{
      Native::Binding{.Set = 1,
                      .Slot = 0,
                      .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&frame, 1))}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value = Native::SampledTexture{.Image = {.Image = overlay.Atlas()},
                                                      .Sampling = Smooth}}};
  const std::array vertices{
      Native::VertexBuffer{.Stream = 0,
                           .Data = overlay.Vertices(),
                           .Range = {.Bytes = overlay.Vertices()->Description().Bytes}}};
  return into.Commands->Draw({.State = {.Pipeline = Pipe.get(),
                                        .Bindings = bindings,
                                        .Vertices = vertices,
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 6, .Instances = overlay.Held()}});
}
}
