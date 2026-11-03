#include "ClipmapNormalPass.h"
#include "ClipmapHeightCache.h"
#include "ComputeShaders.h"
#include "PipelineFactory.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
struct alignas(16) NormalRegion {
  std::array<uint32_t, 4> Area{};
  std::array<uint32_t, 2> Field{};
  float InverseTwoStep = 0;
  float Padding = 0;
};

static_assert(sizeof(NormalRegion) == 32 && alignof(NormalRegion) == 16 &&
              offsetof(NormalRegion, Area) == 0 && offsetof(NormalRegion, Field) == 16 &&
              offsetof(NormalRegion, InverseTwoStep) == 16 + sizeof(NormalRegion::Field) &&
              offsetof(NormalRegion, Padding) == 16 + sizeof(NormalRegion::Field) + sizeof(float));
constexpr auto kKernel = FindComputeShader(ComputeShaderId::ClipmapNormals)->Shape;

uint32_t Before(int64_t coordinate, uint32_t side) {
  const int64_t at = coordinate % side - 1;
  return static_cast<uint32_t>(at < 0 ? at + side : at);
}

bool Matches(const Native::Texture &texture,
             const ClipmapHeightCache &cache,
             Native::Format format,
             Native::TextureUse use) {
  const auto &shape = texture.Description();
  return shape.Type == Native::TextureType::Image2DArray && shape.Texels == format &&
         shape.Width == cache.Side() && shape.Height == cache.Side() &&
         shape.Layers >= cache.Grid().Levels &&
         (static_cast<uint32_t>(shape.Use) & static_cast<uint32_t>(use)) != 0;
}
}

Native::Result<void> ClipmapNormalPass::Configure(Native::Device &device) {
  auto pipeline = CreateComputePipeline(device, ComputeShaderId::ClipmapNormals);
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  auto sampling = device.CreateSampler({.Min = Native::Filter::Nearest,
                                        .Mag = Native::Filter::Nearest,
                                        .Mip = Native::Filter::Nearest,
                                        .MaxLod = 0});
  if (!sampling) { return std::unexpected(sampling.error()); }
  Pipeline_ = std::move(*pipeline);
  Sampling_ = std::move(*sampling);
  return {};
}

Native::Result<void> ClipmapNormalPass::Encode(Native::Commands &commands,
                                               const Native::Texture &heights,
                                               Native::Texture &normals,
                                               const ClipmapHeightCache &cache,
                                               const ClipmapHeightUpdate &update) const {
  const auto &grid = cache.Grid();
  const double firstScale = 0.5 / grid.CellM;
  const double lastScale = std::ldexp(firstScale, 1 - static_cast<int>(grid.Levels));
  if (!Pipeline_ || !Sampling_ || !cache.Validate(update) ||
      !Matches(heights, cache, Native::Format::R32Float, Native::TextureUse::Sampled) ||
      !Matches(normals, cache, Native::Format::Rgba16Float, Native::TextureUse::Storage) ||
      cache.Side() > std::numeric_limits<int32_t>::max() || !std::isfinite(firstScale) ||
      firstScale > std::numeric_limits<float>::max() ||
      lastScale < std::numeric_limits<float>::min()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid clipmap normal field or update"});
  }
  for (const auto &region : update.Regions()) {
    const NormalRegion over{.Area = {Before(region.Low[0], cache.Side()),
                                     Before(region.Low[1], cache.Side()),
                                     static_cast<uint32_t>(std::min(uint64_t{region.Width} + 2,
                                                                    uint64_t{cache.Side()})),
                                     static_cast<uint32_t>(std::min(uint64_t{region.Height} + 2,
                                                                    uint64_t{cache.Side()}))},
                            .Field = {cache.Side(), region.Level},
                            .InverseTwoStep = static_cast<float>(
                                std::ldexp(firstScale, -static_cast<int>(region.Level)))};
    const std::array bindings{
        Native::Binding{
            .Set = 0,
            .Slot = 0,
            .Value = Native::SampledTexture{.Image = {.Image = &heights,
                                                      .Layers = heights.Description().Layers},
                                            .Sampling = Sampling_.get()}},
        Native::Binding{
            .Set = 1,
            .Slot = 0,
            .Value = Native::StorageTexture{.Image = {.Image = &normals,
                                                      .Layers = normals.Description().Layers}}},
        Native::Binding{.Set = 2,
                        .Slot = 0,
                        .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&over, 1))}}};
    const auto encoded =
        commands.Dispatch(*Pipeline_,
                          bindings,
                          {.X = (over.Area[2] + kKernel.GroupX - 1) / kKernel.GroupX,
                           .Y = (over.Area[3] + kKernel.GroupY - 1) / kKernel.GroupY,
                           .Z = 1});
    if (!encoded) { return encoded; }
  }
  return {};
}
}
