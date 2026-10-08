#include "DepthPyramidStage.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ShaderFile.h"
#include <utility>

#include <Extent.h>

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

bool DepthPyramidStage::Configure(const Gpu &gpu,
                                  SDL_GPUTexture *depth,
                                  SDL_GPUSampler *held,
                                  SDL_GPUBuffer *into,
                                  Extent size,
                                  std::string &error) {
  Depth_ = depth;
  Held_ = held;
  Into_ = into;
  Wide_ = static_cast<uint32_t>(size.WidthPx > 0 ? size.WidthPx : 0);
  High_ = static_cast<uint32_t>(size.HeightPx > 0 ? size.HeightPx : 0);
  Shape_ = PyramidOver({.WidthPx = Wide_, .HeightPx = High_});
  if (Depth_ == nullptr || Held_ == nullptr || Into_ == nullptr || Wide_ == 0 || High_ == 0) {
    error = "the depth pyramid needs the frame's depth, a sampler and a buffer of its own, and "
            "the plan did not hold all three";
    return false;
  }
  if (Pipe) { return true; }

  auto made = CreateComputePipeline(gpu.Device, Shader);
  if (!made) {
    error = std::move(made.error());
    return false;
  }
  Pipe = std::move(*made);
  return true;
}

void DepthPyramidStage::Encode(const PassRecording &into) {
  if (!Stands() || into.Dispatch == nullptr) { return; }
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  const SDL_GPUTextureSamplerBinding bound{.texture = Depth_, .sampler = Held_};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, &bound, 1);
  const Reducing over{
      .Source = {Wide_, High_, 0, 0}, .Wide = Shape_.Wide, .High = Shape_.High, .At = Shape_.At};
  SDL_PushGPUComputeUniformData(into.Commands, 0, &over, static_cast<uint32_t>(sizeof over));
  SDL_DispatchGPUCompute(into.Dispatch,
                         (Shape_.Wide[0] + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (Shape_.High[0] + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                         1u);
}

}
