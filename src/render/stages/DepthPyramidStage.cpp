#include "DepthPyramidStage.h"

#include <cstdint>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

namespace {

struct Reducing {
  uint32_t SrcWide = 0, SrcHigh = 0, DstWide = 0, DstHigh = 0;
  uint32_t DstAt = 0, Block = 0, Pad0 = 0, Pad1 = 0;
};

} // namespace

std::string DepthPyramidStage::KernelSource(std::string &error) {
  return ShaderText().Begins().Reads("src/render/shaders/depthPyramid.msl").Take(error);
}

bool DepthPyramidStage::Configure(const Gpu &gpu,
                                  SDL_GPUTexture *depth,
                                  SDL_GPUSampler *held,
                                  SDL_GPUBuffer *into,
                                  int widePx,
                                  int highPx,
                                  std::string &error) {
  Depth_ = depth;
  Held_ = held;
  Into_ = into;
  Wide_ = static_cast<uint32_t>(widePx > 0 ? widePx : 0);
  High_ = static_cast<uint32_t>(highPx > 0 ? highPx : 0);
  Shape_ = PyramidOver({.WidthPx = Wide_, .HeightPx = High_});
  if (Depth_ == nullptr || Held_ == nullptr || Into_ == nullptr || Wide_ == 0 || High_ == 0) {
    error = "the depth pyramid needs the frame's depth, a sampler and a buffer of its own, and "
            "the plan did not hold all three";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "depthPyramidKernel";
  wanted.num_samplers = KernelShape.Samplers;
  wanted.num_readwrite_storage_buffers = KernelShape.ReadWriteBuffers;
  wanted.num_uniform_buffers = KernelShape.UniformBuffers;
  wanted.threadcount_x = KernelShape.GroupX;
  wanted.threadcount_y = KernelShape.GroupY;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the depth pyramid kernel was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  return true;
}

void DepthPyramidStage::Encode(const PassRecording &into) {
  if (!Stands() || into.Dispatch == nullptr) { return; }
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  const SDL_GPUTextureSamplerBinding bound{.texture = Depth_, .sampler = Held_};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, &bound, 1);
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    Reducing over;
    over.SrcWide = Wide_;
    over.SrcHigh = High_;
    over.DstWide = Shape_.Wide[level];
    over.DstHigh = Shape_.High[level];
    over.DstAt = Shape_.At[level];
    over.Block = 2u << level;
    SDL_PushGPUComputeUniformData(into.Commands, 0, &over, static_cast<uint32_t>(sizeof over));
    SDL_DispatchGPUCompute(into.Dispatch,
                           (over.DstWide + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                           (over.DstHigh + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                           1u);
  }
}

} // namespace outshine::Render
