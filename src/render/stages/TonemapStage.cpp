#include "math/Vec2.h"
#include "TonemapStage.h"
#include "math/Vec3.h"

#include "ShaderFile.h"
#include <array>
#include <cstdint>
#include <string>

namespace outshine::Render {

namespace {

constexpr uint32_t kTonemapImages = TonemapStage::ShaderShape.FragmentSamplers;
constexpr uint32_t kTemporalImages = TonemapStage::TemporalShaderShape.FragmentSamplers;

}

bool TonemapStage::Configure(const Gpu &gpu,
                             const Feeds &from,
                             const DisplayOptions &options,
                             std::string &error) {
  Scene = from.Scene;
  Depth = from.Depth;
  Exact = from.Exact;

  Temporal = options.Temporal;
  Display_ = options;
  const DrawShape &shape = options.Temporal ? TemporalShaderShape : ShaderShape;
  const OwnedShader vertex(gpu.Device,
                           ShaderFrom(gpu.Device,
                                      "build/shaders/fullscreen.vert.spv",
                                      SDL_GPU_SHADERSTAGE_VERTEX,
                                      shape,
                                      error));
  const OwnedShader fragment(gpu.Device,
                             ShaderFrom(gpu.Device,
                                        options.Temporal ? "build/shaders/temporalResolve.frag.spv"
                                                         : "build/shaders/tonemap.frag.spv",
                                        SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        shape,
                                        error));
  if (!vertex || !fragment) { return false; }

  std::array<SDL_GPUColorTargetDescription, 2> target = {{}};
  target[0].format = options.Temporal ? from.Linear : gpu.SurfaceFormat;
  target[1].format = gpu.SurfaceFormat;
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = target.data();
  pipeline.target_info.num_color_targets = options.Temporal ? 2u : 1u;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (made == nullptr) {
    error = std::string("the display transfer's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void TonemapStage::Encode([[maybe_unused]] const FrameContext &ctx, const PassRecording &into) {
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
    const std::array<SDL_GPUTextureSamplerBinding, kTemporalImages> images = {
        {{.texture = Scene, .sampler = Exact},
         {.texture = Depth, .sampler = Exact},
         {.texture = History, .sampler = Exact},
         {.texture = Velocity, .sampler = Exact}}};
    SDL_BindGPUFragmentSamplers(into.Pass, 0, images.data(), kTemporalImages);
  } else {
    const std::array<SDL_GPUTextureSamplerBinding, kTonemapImages> images = {
        {{.texture = Scene, .sampler = Exact}, {.texture = Depth, .sampler = Exact}}};
    SDL_BindGPUFragmentSamplers(into.Pass, 0, images.data(), kTonemapImages);
  }

  const struct {
    float Exposure;
    uint32_t Filmic;
    std::array<float, 2> Pad;
  } display{.Exposure = Display_.Exposure,
            .Filmic = Display_.Curve == Transfer::Filmic ? 1u : 0u,
            .Pad = {}};

  static_assert(sizeof(display) == 16);
  SDL_PushGPUFragmentUniformData(into.Commands, Temporal ? 1u : 0u, &display, sizeof display);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

}
