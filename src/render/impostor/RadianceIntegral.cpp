#include "RadianceIntegral.h"
#include "PipelineFactory.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <utility>

namespace outshine::Render {
namespace {
bool Uses(const Native::TextureDescription &image, Native::TextureUse use) {
  return (static_cast<uint32_t>(image.Use) & static_cast<uint32_t>(use)) != 0;
}

bool Image(const Native::TextureDescription &image) {
  return image.Type == Native::TextureType::Image2D && image.Layers == 1 && image.Width > 0 &&
         image.Height > 0;
}

bool Valid(const Native::Texture &source,
           const Native::Texture &scratch,
           const Native::Texture &target) {
  const auto &from = source.Description();
  const auto &rows = scratch.Description();
  const auto &to = target.Description();
  return &source != &scratch && &source != &target && &scratch != &target && Image(from) &&
         Image(rows) && Image(to) &&
         (from.Texels == Native::Format::Rgba16Float ||
          from.Texels == Native::Format::Rgba32Float) &&
         rows.Texels == Native::Format::Rgba32Float && to.Texels == rows.Texels &&
         2 * (uint64_t{from.Width} + 1) == to.Width && uint64_t{from.Height} + 1 == to.Height &&
         rows.Width == to.Width && rows.Height == to.Height &&
         Uses(from, Native::TextureUse::Sampled) && Uses(rows, Native::TextureUse::Sampled) &&
         Uses(rows, Native::TextureUse::Storage) && Uses(to, Native::TextureUse::Storage);
}
}

std::optional<Extent> RadianceIntegral::StorageExtent(Extent raw, uint32_t textureLimit) noexcept {
  if (raw.WidthPx <= 0 || raw.HeightPx <= 0) { return {}; }
  const uint64_t width = 2 * (uint64_t{static_cast<uint32_t>(raw.WidthPx)} + 1);
  const uint64_t height = uint64_t{static_cast<uint32_t>(raw.HeightPx)} + 1;
  const auto limit = std::min(uint64_t{textureLimit}, uint64_t{std::numeric_limits<int>::max()});
  if (width > limit || height > limit) { return {}; }
  return Extent{.WidthPx = static_cast<int>(width), .HeightPx = static_cast<int>(height)};
}

Native::Result<void> RadianceIntegral::Configure(Native::Device &device) {
  if (Device_ != nullptr) {
    if (Device_ != &device) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "radiance integral belongs to another device"});
    }
    return {};
  }
  auto rows = CreateComputePipeline(device, ComputeShaderId::RadianceIntegralRows);
  if (!rows) { return std::unexpected(rows.error()); }
  auto columns = CreateComputePipeline(device, ComputeShaderId::RadianceIntegralColumns);
  if (!columns) { return std::unexpected(columns.error()); }
  auto sampling = device.CreateSampler({.Min = Native::Filter::Nearest,
                                        .Mag = Native::Filter::Nearest,
                                        .Mip = Native::Filter::Nearest,
                                        .U = Native::Address::ClampToEdge,
                                        .V = Native::Address::ClampToEdge,
                                        .MinLod = 0,
                                        .MaxLod = 0});
  if (!sampling) { return std::unexpected(sampling.error()); }
  if (!*sampling) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                         .Message = "radiance integral has no sampler"});
  }
  Rows_ = std::move(*rows);
  Columns_ = std::move(*columns);
  Sampling_ = std::move(*sampling);
  Device_ = &device;
  return {};
}

Native::Result<void> RadianceIntegral::Encode(Native::Commands &commands,
                                              const Native::Texture &source,
                                              Native::Texture &scratch,
                                              Native::Texture &target) const {
  if (!Rows_ || !Columns_ || !Sampling_ || !Valid(source, scratch, target)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "radiance integral needs distinct padded Float32 pair images"});
  }
  const auto dispatch = [&](const Native::ComputePipeline &pipeline,
                            const Native::Texture &input,
                            Native::Texture &output,
                            uint32_t lines) {
    const std::array bindings{
        Native::Binding{.Set = 0,
                        .Slot = 0,
                        .Value = Native::SampledTexture{.Image = {.Image = &input},
                                                        .Sampling = Sampling_.get()}},
        Native::Binding{
            .Set = 1, .Slot = 0, .Value = Native::StorageTexture{.Image = {.Image = &output}}}};
    constexpr auto rows = FindComputeShader(ComputeShaderId::RadianceIntegralRows)->Shape;
    constexpr auto columns = FindComputeShader(ComputeShaderId::RadianceIntegralColumns)->Shape;
    static_assert(rows.GroupX == columns.GroupX && rows.GroupY == 1 && rows.GroupZ == 1 &&
                  columns.GroupY == 1 && columns.GroupZ == 1);
    return commands.Dispatch(pipeline, bindings, {.X = (lines + rows.GroupX - 1) / rows.GroupX});
  };
  if (const auto rows = dispatch(*Rows_, source, scratch, target.Description().Height); !rows) {
    return std::unexpected(rows.error());
  }
  return dispatch(*Columns_, scratch, target, target.Description().Width / 2);
}
}
