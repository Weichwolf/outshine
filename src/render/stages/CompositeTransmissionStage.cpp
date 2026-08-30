#include "CompositeTransmissionStage.h"

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

constexpr uint32_t kCompositeImages = CompositeTransmissionStage::ShaderShape.FragmentSamplers;

}

bool CompositeTransmissionStage::Configure(const Gpu &gpu,
                                           SDL_GPUTexture *opaque,
                                           SDL_GPUTexture *transmissive,
                                           SDL_GPUSampler *exact,
                                           SDL_GPUTextureFormat targetFormat,
                                           std::string &error) {
  Opaque = opaque;
  Transmissive = transmissive;
  Exact = exact;

  const std::string source = ShaderSource(error);
  if (source.empty()) { return false; }
  const OwnedShader vertex(
      gpu.Device, ShaderFrom(gpu.Device, source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, ShaderShape));
  const OwnedShader fragment(
      gpu.Device, ShaderFrom(gpu.Device, source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, ShaderShape));
  if (!vertex || !fragment) {
    error = std::string("the transmission composite did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUColorTargetDescription target{};
  target.format = targetFormat;
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
    error = std::string("the transmission composite's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void CompositeTransmissionStage::Encode(const FrameContext &, const PassRecording &into) {
  if (!Pipe) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  const SDL_GPUTextureSamplerBinding images[kCompositeImages] = {{Opaque, Exact},
                                                                 {Transmissive, Exact}};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kCompositeImages);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

std::string CompositeTransmissionStage::ShaderSource() {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string CompositeTransmissionStage::ShaderSource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/compositeTransmission.msl", body, error)) {
    return std::string();
  }
  return MslPrelude(error) + body;
}

} // namespace outshine::Render
