#include "DepthPyramidStage.h"
#include "StagePipelines.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
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
static_assert(kPyramidLevels == 4 && DepthPyramidStage::KernelShape.GroupX == 8 &&
              DepthPyramidStage::KernelShape.GroupY == 8 &&
              DepthPyramidStage::KernelShape.GroupZ == 1);
}

Native::Result<void> DepthPyramidStage::Configure(const Gpu &gpu,
                                                  Native::Texture *depth,
                                                  Native::Sampler *held,
                                                  Native::Buffer *into,
                                                  Extent size) {
  if (gpu.Device == nullptr || depth == nullptr || held == nullptr || into == nullptr ||
      size.WidthPx <= 0 || size.HeightPx <= 0 ||
      std::cmp_greater(size.WidthPx, depth->Description().Width) ||
      std::cmp_greater(size.HeightPx, depth->Description().Height)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid needs valid source bounds"});
  }
  const auto wide = static_cast<uint32_t>(size.WidthPx);
  const auto high = static_cast<uint32_t>(size.HeightPx);
  const auto shape = PyramidOver({.WidthPx = wide, .HeightPx = high});
  if (into->Description().Bytes < uint64_t{shape.Texels} * sizeof(float)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid exceeds output storage"});
  }
  if (!Pipe) {
    auto made = CreateComputePipeline(*gpu.Device, Shader);
    if (!made) { return std::unexpected(made.error()); }
    Pipe = std::move(*made);
  }
  Depth_ = depth;
  Held_ = held;
  Into_ = into;
  Wide_ = wide;
  High_ = high;
  Shape_ = shape;
  return {};
}

Native::Result<void> DepthPyramidStage::Encode(const PassRecording &into) {
  if (!Stands() || Depth_ == nullptr || Held_ == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "depth pyramid is not configured"});
  }
  const Reducing over{
      .Source = {Wide_, High_, 0, 0}, .Wide = Shape_.Wide, .High = Shape_.High, .At = Shape_.At};
  const std::array bindings{
      Native::Binding{.Set = 0,
                      .Slot = 0,
                      .Value =
                          Native::SampledTexture{.Image = {.Image = Depth_}, .Sampling = Held_}},
      Native::Binding{
          .Set = 1,
          .Slot = 0,
          .Value =
              Native::StorageBuffer{.Data = Into_, .Range = {.Bytes = Into_->Description().Bytes}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&over, size_t{1}})}}};
  return into.Commands->Dispatch(
      *Pipe,
      bindings,
      {.X = (Shape_.Wide[0] + KernelShape.GroupX - 1u) / KernelShape.GroupX,
       .Y = (Shape_.High[0] + KernelShape.GroupY - 1u) / KernelShape.GroupY,
       .Z = 1u});
}

}
