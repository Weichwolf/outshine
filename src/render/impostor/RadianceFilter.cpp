#include "RadianceFilter.h"
#include "PipelineFactory.h"
#include <array>
#include <cstdint>
#include <expected>
#include <utility>

namespace outshine::Render {
Native::Result<void> RadianceFilter::Configure(Native::Device &device) {
  if (Device_ != nullptr) {
    if (Device_ != &device) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "radiance filter belongs to another device"});
    }
    return {};
  }
  auto pipeline = CreateComputePipeline(device, ComputeShaderId::RadianceFilter);
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  auto sampler = device.CreateSampler({.Min = Native::Filter::Nearest,
                                       .Mag = Native::Filter::Nearest,
                                       .Mip = Native::Filter::Nearest,
                                       .U = Native::Address::ClampToEdge,
                                       .V = Native::Address::ClampToEdge,
                                       .MinLod = 0,
                                       .MaxLod = 0});
  if (!sampler) { return std::unexpected(sampler.error()); }
  if (!*sampler) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Driver,
                      .Message = "radiance sampler allocation returned no sampler"});
  }
  Pipeline_ = std::move(*pipeline);
  Sampler_ = std::move(*sampler);
  Device_ = &device;
  return {};
}

Native::Result<void> RadianceFilter::Encode(Native::Commands &commands,
                                            const Native::Texture &source,
                                            Native::Texture &destination) const {
  const auto &from = source.Description();
  const auto &to = destination.Description();
  if (!Pipeline_ || !Sampler_ || &source == &destination ||
      from.Type != Native::TextureType::Image2D || to.Type != Native::TextureType::Image2D ||
      from.Layers != 1 || to.Layers != 1 || from.Width == 0 || from.Height == 0 || to.Width == 0 ||
      to.Height == 0 || from.Width % to.Width != 0 || from.Height % to.Height != 0 ||
      from.Texels != Native::Format::Rgba16Float || to.Texels != Native::Format::Rgba16Float ||
      (static_cast<uint32_t>(from.Use) & static_cast<uint32_t>(Native::TextureUse::Sampled)) == 0 ||
      (static_cast<uint32_t>(to.Use) & static_cast<uint32_t>(Native::TextureUse::Storage)) == 0) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "radiance filter needs distinct linear HDR integer footprints"});
  }
  const std::array bindings{
      Native::Binding{
          .Set = 0,
          .Slot = 0,
          .Value = Native::SampledTexture{.Image = {.Image = &source}, .Sampling = Sampler_.get()}},
      Native::Binding{
          .Set = 1, .Slot = 0, .Value = Native::StorageTexture{.Image = {.Image = &destination}}}};
  constexpr auto shape = FindComputeShader(ComputeShaderId::RadianceFilter)->Shape;
  return commands.Dispatch(*Pipeline_,
                           bindings,
                           {.X = (to.Width + shape.GroupX - 1) / shape.GroupX,
                            .Y = (to.Height + shape.GroupY - 1) / shape.GroupY,
                            .Z = 1});
}
}
