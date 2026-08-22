#include "PresentStage.h"

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {

}

bool PresentStage::Configure(const Gpu &gpu, SDL_GPUTexture *frame, SDL_GPUSampler *exact,
                             std::string &error) {
  Frame = frame;
  Exact = exact;
  if (Frame == nullptr) {
    error = "there is no frame to present, so the plan holds no picture to put on a surface";
    return false;
  }

  (void)gpu;
  return true;
}

bool PresentStage::For(const Gpu &gpu, SDL_GPUTextureFormat surfaceFormat, std::string &error) {
  if (Pipe && Built == surfaceFormat) { return true; }

  const std::string source = ShaderSource(error);
  if (source.empty()) { return false; }
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  wanted.num_samplers = ShaderShape.VertexSamplers;
  wanted.num_uniform_buffers = ShaderShape.VertexUniformBuffers;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = ShaderShape.FragmentSamplers;
  wanted.num_uniform_buffers = ShaderShape.FragmentUniformBuffers;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the present did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUColorTargetDescription target{};
  target.format = surfaceFormat;
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = &target;
  pipeline.target_info.num_color_targets = 1;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (!made) {
    error = std::string("the present's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  Built = surfaceFormat;
  return true;
}

void PresentStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  if (!Pipe || Frame == nullptr) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  SDL_GPUTextureSamplerBinding bound{Frame, Exact};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, &bound, 1);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

std::string PresentStage::ShaderSource(void) {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string PresentStage::ShaderSource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/present.msl", body, error)) { return std::string(); }
  return std::string(kMslPrelude) + body;
}

}
