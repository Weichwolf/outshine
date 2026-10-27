#include "DepthPyramidPass.h"
#include "PipelineFactory.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
struct alignas(16) Reducing {
  std::array<uint32_t, 4> Source{}, Wide{}, High{}, At{};
};

static_assert(sizeof(Reducing) == 64 && alignof(Reducing) == 16);
static_assert(offsetof(Reducing, Source) == 0 && offsetof(Reducing, Wide) == 16 &&
              offsetof(Reducing, High) == 32 &&
              offsetof(Reducing, At) == 3 * sizeof(Reducing::Source));
static_assert(kPyramidLevels == 4 && DepthPyramidPass::KernelShape.GroupX == 8 &&
              DepthPyramidPass::KernelShape.GroupY == 8 &&
              DepthPyramidPass::KernelShape.GroupZ == 1);
}

Native::Result<void> DepthPyramidPass::Allocate(Native::Device &device, Extent size) {
  const auto &limits = device.Limits();
  if (size.WidthPx <= 0 || size.HeightPx <= 0 ||
      std::cmp_greater(size.WidthPx, limits.MaxTextureExtent) ||
      std::cmp_greater(size.HeightPx, limits.MaxTextureExtent)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid needs valid source bounds"});
  }
  const auto wide = static_cast<uint32_t>(size.WidthPx);
  const auto high = static_cast<uint32_t>(size.HeightPx);
  if (Buffer_) {
    if (Device_ != &device || Shape_.SourceWide != wide || Shape_.SourceHigh != high) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "depth pyramid already belongs to another device or extent"});
    }
    return {};
  }
  uint64_t across = wide;
  uint64_t down = high;
  uint64_t texels = 0;
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    across = across / 2u + across % 2u;
    down = down / 2u + down % 2u;
    texels += across * down;
  }
  if (texels > std::numeric_limits<uint32_t>::max() ||
      texels * sizeof(float) > limits.MaxStorageBufferBytes) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid exceeds device storage"});
  }
  const auto shape = PyramidOver({.WidthPx = wide, .HeightPx = high});
  auto made =
      device.CreateBuffer({.Bytes = uint64_t{shape.Texels} * sizeof(float),
                           .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource});
  if (!made) { return std::unexpected(made.error()); }
  if (!*made) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                         .Message = "depth pyramid allocation returned no buffer"});
  }
  Buffer_ = std::move(*made);
  Device_ = &device;
  Shape_ = shape;
  return {};
}

Native::Result<void> DepthPyramidPass::Configure(Native::Texture *depth, Native::Sampler *held) {
  if (!Buffer_ || depth == nullptr || held == nullptr ||
      Shape_.SourceWide > depth->Description().Width ||
      Shape_.SourceHigh > depth->Description().Height) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid needs valid source bounds"});
  }
  if (!Pipeline_) {
    auto made = CreateComputePipeline(*Device_, Shader);
    if (!made) { return std::unexpected(made.error()); }
    Pipeline_ = std::move(*made);
  }
  SourceDepth_ = depth;
  Sampler_ = held;
  return {};
}

Native::Result<void> DepthPyramidPass::Encode(const PassRecording &into) {
  if (!Ready() || SourceDepth_ == nullptr || Sampler_ == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid is not configured"});
  }
  const Reducing over{.Source = {Shape_.SourceWide, Shape_.SourceHigh, 0, 0},
                      .Wide = Shape_.Wide,
                      .High = Shape_.High,
                      .At = Shape_.At};
  const std::array bindings{
      Native::Binding{
          .Set = 0,
          .Slot = 0,
          .Value = Native::SampledTexture{.Image = {.Image = SourceDepth_}, .Sampling = Sampler_}},
      Native::Binding{.Set = 1,
                      .Slot = 0,
                      .Value =
                          Native::StorageBuffer{.Data = Buffer_.get(),
                                                .Range = {.Bytes = Buffer_->Description().Bytes}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&over, size_t{1}})}}};
  return into.Commands->Dispatch(
      *Pipeline_,
      bindings,
      {.X = (Shape_.Wide[0] + KernelShape.GroupX - 1u) / KernelShape.GroupX,
       .Y = (Shape_.High[0] + KernelShape.GroupY - 1u) / KernelShape.GroupY,
       .Z = 1u});
}

}
