#include "PresentStage.h"

#include "ShaderFile.h"
#include "ShaderPrelude.h"
#include <string>

namespace outshine::Render {
namespace {}

bool PresentStage::Configure(const Gpu &gpu,
                             SDL_GPUTexture *frame,
                             SDL_GPUSampler *exact,
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
  const OwnedShader vertex(
      gpu.Device, ShaderFrom(gpu.Device, source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, ShaderShape));
  const OwnedShader fragment(
      gpu.Device, ShaderFrom(gpu.Device, source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, ShaderShape));
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
  if (made == nullptr) {
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
  const SDL_GPUTextureSamplerBinding bound{.texture = Frame, .sampler = Exact};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, &bound, 1);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

std::string PresentStage::ShaderSource() {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string PresentStage::ShaderSource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/present.msl", body, error)) { return {}; }
  return MslPrelude(error) + body;
}

} // namespace outshine::Render
