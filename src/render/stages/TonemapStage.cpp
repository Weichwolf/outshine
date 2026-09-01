#include "math/Vec2.h"
#include "TonemapStage.h"
#include "math/Vec3.h"

#include "ShaderFile.h"
#include "ShaderPrelude.h"
#include <cstdint>
#include <string>

namespace outshine::Render {

namespace {

constexpr uint32_t kTonemapImages = TonemapStage::ShaderShape.FragmentSamplers;
constexpr uint32_t kTemporalImages = TonemapStage::TemporalShaderShape.FragmentSamplers;

} // namespace

std::string TonemapStage::ShaderSource(const DisplayOptions &options) {
  std::string ignored;
  return ShaderSource(options, ignored);
}

std::string TonemapStage::ShaderSource(const DisplayOptions &options, std::string &error) {
  std::string body;
  if (!LoadShaderText(options.Temporal ? "src/render/shaders/temporalResolve.msl"
                                       : "src/render/shaders/tonemap.msl",
                      body,
                      error)) {
    return std::string();
  }
  return MslPrelude(error) + DisplayMsl(options) + body;
}

bool TonemapStage::Configure(const Gpu &gpu,
                             SDL_GPUTexture *scene,
                             SDL_GPUTexture *depth,
                             SDL_GPUSampler *exact,
                             SDL_GPUTextureFormat linear,
                             const DisplayOptions &options,
                             std::string &error) {
  Scene = scene;
  Depth = depth;
  Exact = exact;

  Temporal = options.Temporal;
  const std::string source = ShaderSource(options, error);
  if (source.empty()) { return false; }
  const DrawShape &shape = options.Temporal ? TemporalShaderShape : ShaderShape;
  const OwnedShader vertex(gpu.Device,
                           ShaderFrom(gpu.Device, source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, shape));
  const OwnedShader fragment(
      gpu.Device, ShaderFrom(gpu.Device, source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, shape));
  if (!vertex || !fragment) {
    error = std::string("the display transfer did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUColorTargetDescription target[2] = {};
  target[0].format = options.Temporal ? linear : gpu.SurfaceFormat;
  target[1].format = gpu.SurfaceFormat;
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = target;
  pipeline.target_info.num_color_targets = options.Temporal ? 2u : 1u;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (made == nullptr) {
    error = std::string("the display transfer's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void TonemapStage::Encode(const FrameContext &, const PassRecording &into) {
  if (!Pipe) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  if (Temporal) {
    if (History == nullptr || Velocity == nullptr) { return; }

    struct {
      Vec2f JitterDelta;
      Vec2f Texel;
      float HistoryHeld;
      Vec3f Pad;
    } uniforms{.JitterDelta = {JitterDelta[0], JitterDelta[1]},
               .Texel = {Width > 0 ? 1.0f / static_cast<float>(Width) : 0.0f,
                         Height > 0 ? 1.0f / static_cast<float>(Height) : 0.0f},
               .HistoryHeld = HistoryHeld ? 1.0f : 0.0f,
               .Pad = {0.0f, 0.0f, 0.0f}};

    SDL_PushGPUFragmentUniformData(into.Commands, 0, &uniforms, sizeof uniforms);
    const SDL_GPUTextureSamplerBinding images[kTemporalImages] = {
        {.texture = Scene, .sampler = Exact},
        {.texture = Depth, .sampler = Exact},
        {.texture = History, .sampler = Exact},
        {.texture = Velocity, .sampler = Exact}};
    SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kTemporalImages);
  } else {
    const SDL_GPUTextureSamplerBinding images[kTonemapImages] = {
        {.texture = Scene, .sampler = Exact}, {.texture = Depth, .sampler = Exact}};
    SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kTonemapImages);
  }
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

} // namespace outshine::Render
