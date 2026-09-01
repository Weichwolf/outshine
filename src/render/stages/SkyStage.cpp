#include "SkyStage.h"

#include <cstdint>
#include <numbers>
#include <cstdio>
#include <string>

#include "SceneTargets.h"
#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {}

bool SkyStage::Configure(const Gpu &gpu,
                         SDL_GPUTexture *skyView,
                         SDL_GPUTexture *transmittance,
                         SDL_GPUSampler *lut,
                         std::string &error) {
  SkyView = skyView;
  Veil = transmittance;
  Lut = lut;
  if (SkyView == nullptr || Veil == nullptr || Lut == nullptr) {
    error = "the sky draw needs the sky view table and its sampler, and the plan did not hold both";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = ShaderSource(error);
  if (source.empty()) { return false; }
  const OwnedShader vertex(
      gpu.Device, ShaderFrom(gpu.Device, source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, ShaderShape));
  const OwnedShader fragment(
      gpu.Device, ShaderFrom(gpu.Device, source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, ShaderShape));
  if (!vertex || !fragment) {
    error = std::string("the sky did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUColorTargetDescription targets[2] = {};
  targets[0].format = gpu.HdrFormat;
  targets[1] = VelocityTarget(true);
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = targets;
  pipeline.target_info.num_color_targets = 2;
  pipeline.target_info.has_depth_stencil_target = true;
  pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  pipeline.depth_stencil_state.enable_depth_test = false;
  pipeline.depth_stencil_state.enable_depth_write = false;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (made == nullptr) {
    error = std::string("the sky's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void SkyStage::Declare(const Medium &medium,
                       const float sunDir[3],
                       const float up[3],
                       float illuminanceLux,
                       float eyeHeightM) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.SunDir[axis] = sunDir[axis];
    Pushed_.WorldUp[axis] = up[axis];
  }
  Pushed_.Illuminance = illuminanceLux;
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / 1000.0f;
  Pushed_.BottomRadiusKm = medium.BottomRadiusKm;
  Pushed_.SunHalfAngleRad = kSunHalfAngleRad;
  Pushed_.Air = medium;
  Pushed_.TopRadiusKm = medium.TopRadiusKm;
  Declared_ = true;
}

void SkyStage::Eye(const Medium &medium, float eyeHeightM) {
  if (!Declared_) { return; }
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / 1000.0f;
}

void SkyStage::SetBasis(
    const Vec3f &right, const Vec3f &upAxis, const Vec3f &fwd, float tanHalfW, float tanHalfH) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.Right[axis] = right[axis];
    Pushed_.Up[axis] = upAxis[axis];
    Pushed_.Fwd[axis] = fwd[axis];
  }
  Pushed_.TanHalf[0] = tanHalfW;
  Pushed_.TanHalf[1] = tanHalfH;
}

void SkyStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  if (!Pipe || !Declared_ || into.Pass == nullptr) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  const SDL_GPUTextureSamplerBinding bound[2] = {{.texture = SkyView, .sampler = Lut},
                                                 {.texture = Veil, .sampler = Lut}};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, bound, 2);
  SDL_PushGPUFragmentUniformData(into.Commands, 0, &Pushed_, static_cast<uint32_t>(sizeof Pushed_));
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

std::string SkyStage::ShaderSource() {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string SkyStage::ShaderSource(std::string &error) {
  std::string layout;
  std::string core;
  std::string body;
  if (!LoadShaderText("src/render/shaders/mediumLayout.msl", layout, error) ||
      !LoadShaderText("src/render/stages/MediumCore.h", core, error) ||
      !LoadShaderText("src/render/shaders/sky.msl", body, error)) {
    return {};
  }
  return MslPrelude(error) + VelocityStaticDefine() +
         "#define MEDIUM_CONST constant\n#define MEDIUM_THREAD thread\n" + layout + core + body;
}

} // namespace outshine::Render
