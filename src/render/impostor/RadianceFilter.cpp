#include "RadianceFilter.h"
#include "PipelineFactory.h"
#include <array>
#include <bit>
#include <cstdint>
#include <expected>
#include <memory>
#include <utility>

namespace outshine::Render {
namespace {
struct FilterImages {
  const Native::TextureDescription &Source;
  const Native::TextureDescription &Target;
};

bool Valid(FilterImages images) {
  const auto &from = images.Source;
  const auto &to = images.Target;
  return from.Type == Native::TextureType::Image2D && to.Type == Native::TextureType::Image2D &&
         from.Layers == 1 && to.Layers == 1 && from.Width > 0 && from.Height > 0 && to.Width > 0 &&
         to.Height > 0 && from.Width % to.Width == 0 && from.Height % to.Height == 0 &&
         from.Texels == Native::Format::Rgba16Float && to.Texels == Native::Format::Rgba16Float &&
         (static_cast<uint32_t>(from.Use) & static_cast<uint32_t>(Native::TextureUse::Sampled)) !=
             0 &&
         (static_cast<uint32_t>(to.Use) & static_cast<uint32_t>(Native::TextureUse::Storage)) != 0;
}
}

Native::Result<std::shared_ptr<const RadianceImage>>
RadianceFilter::Apply(Native::Device &device, const Native::Texture &source, Extent target) {
  if (target.WidthPx <= 0 || target.HeightPx <= 0) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "radiance target requires an integer footprint"});
  }
  const Native::TextureDescription description{.Width = static_cast<uint32_t>(target.WidthPx),
                                               .Height = static_cast<uint32_t>(target.HeightPx),
                                               .Texels = Native::Format::Rgba16Float,
                                               .Use = Native::TextureUse::Storage |
                                                      Native::TextureUse::Sampled |
                                                      Native::TextureUse::CopySource};
  if (!Valid({.Source = source.Description(), .Target = description})) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "radiance filtering requires linear HDR integer footprints"});
  }
  if (const auto configured = Configure(device); !configured) {
    return std::unexpected(configured.error());
  }
  auto texture = device.CreateTexture(description);
  if (!texture) { return std::unexpected(texture.error()); }
  if (!*texture) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                         .Message = "radiance allocation returned no image"});
  }
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  if (const auto encoded = Encode(**commands, source, **texture); !encoded) {
    return std::unexpected(encoded.error());
  }
  const auto completion = device.Submit(std::move(*commands));
  if (!completion) { return std::unexpected(completion.error()); }
  return std::make_shared<const RadianceImage>(RadianceImage{.Device = &device,
                                                             .Texture = std::move(*texture),
                                                             .Sampling = Sampler_,
                                                             .Produced = *completion});
}

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
      !Valid({.Source = from, .Target = to})) {
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
  constexpr uint32_t lanes = shape.GroupX * shape.GroupY;
  static_assert(shape.GroupZ == 1 && std::has_single_bit(lanes));
  const bool cooperative = uint64_t{from.Width / to.Width} * (from.Height / to.Height) >= lanes;
  return commands.Dispatch(
      *Pipeline_,
      bindings,
      {.X = cooperative ? to.Width : (to.Width + shape.GroupX - 1) / shape.GroupX,
       .Y = cooperative ? to.Height : (to.Height + shape.GroupY - 1) / shape.GroupY,
       .Z = 1});
}
}
