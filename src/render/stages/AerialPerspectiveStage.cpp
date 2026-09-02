#include "AerialPerspectiveStage.h"
#include "math/Vec3.h"

#include "ShaderFile.h"
#include "ShaderPrelude.h"
#include <array>
#include <string>
#include <cstdint>

namespace outshine::Render {

constexpr float kMPerKmF = 1000.0f;

bool AerialPerspectiveStage::Configure(const Gpu &gpu,
                                       SDL_GPUTexture *scene,
                                       SDL_GPUTexture *depth,
                                       SDL_GPUTexture *skyView,
                                       SDL_GPUTexture *transmittance,
                                       SDL_GPUSampler *exact,
                                       SDL_GPUSampler *lut,
                                       SDL_GPUTextureFormat targetFormat,
                                       std::string &error) {
  Scene = scene;
  Depth = depth;
  SkyView = skyView;
  Veil = transmittance;
  Exact = exact;
  Lut = lut;

  const std::string source = ShaderSource(error);
  if (source.empty()) { return false; }
  const OwnedShader vertex(
      gpu.Device, ShaderFrom(gpu.Device, source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, ShaderShape));
  const OwnedShader fragment(
      gpu.Device, ShaderFrom(gpu.Device, source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, ShaderShape));
  if (!vertex || !fragment) {
    error = std::string("the aerial perspective did not compile: ") + SDL_GetError();
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
  if (made == nullptr) {
    error = std::string("the aerial perspective's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void AerialPerspectiveStage::Declare(const Medium &medium,
                                     const Vec3f &sunDir,
                                     const Vec3f &up,
                                     float illuminanceLux,
                                     float eyeHeightM) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.SunDir[axis] = sunDir[axis];
    Pushed_.WorldUp[axis] = up[axis];
  }
  Pushed_.Illuminance = illuminanceLux;
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / kMPerKmF;
  Pushed_.Air = medium;
  Declared_ = true;
}

void AerialPerspectiveStage::Eye(const Medium &medium, float eyeHeightM) {
  if (!Declared_) { return; }
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / kMPerKmF;
}

void AerialPerspectiveStage::SetBasis(
    const Vec3f &right, const Vec3f &upAxis, const Vec3f &fwd, float tanHalfW, float tanHalfH) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.Right[axis] = right[axis];
    Pushed_.Up[axis] = upAxis[axis];
    Pushed_.Fwd[axis] = fwd[axis];
  }
  Pushed_.TanHalf[0] = tanHalfW;
  Pushed_.TanHalf[1] = tanHalfH;
}

void AerialPerspectiveStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  if (!Pipe || !Declared_ || into.Pass == nullptr) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  const std::array<SDL_GPUTextureSamplerBinding, 4> bound = {{{.texture = Scene, .sampler = Exact},
                                                              {.texture = Depth, .sampler = Exact},
                                                              {.texture = SkyView, .sampler = Lut},
                                                              {.texture = Veil, .sampler = Lut}}};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, bound.data(), 4);
  SDL_PushGPUFragmentUniformData(into.Commands, 0, &Pushed_, static_cast<uint32_t>(sizeof Pushed_));
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

std::string AerialPerspectiveStage::ShaderSource() {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string AerialPerspectiveStage::ShaderSource(std::string &error) {
  std::string layout;
  std::string core;
  std::string medium;
  std::string body;
  if (!LoadShaderText("src/render/shaders/mediumLayout.msl", layout, error) ||
      !LoadShaderText("src/render/stages/MediumCore.h", core, error) ||
      !LoadShaderText("src/render/shaders/medium.msl", medium, error) ||
      !LoadShaderText("src/render/shaders/aerialPerspective.msl", body, error)) {
    return {};
  }
  return MslPrelude(error) + "#define MEDIUM_CONST constant\n#define MEDIUM_THREAD thread\n" +
         layout + core + medium + body;
}

} // namespace outshine::Render
