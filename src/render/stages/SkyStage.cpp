#include "SkyStage.h"
#include "math/Vec3.h"

#include <array>
#include <cstdint>
#include <numbers>
#include <cstdio>
#include <string>

#include "SceneTargets.h"
#include "ShaderFile.h"

namespace outshine::Render {

namespace Says {
constexpr auto UnsupportedSkyAttachment = "sky pass has an unsupported colour attachment";
}

constexpr float kMPerKmF = 1000.0f;

bool SkyStage::Configure(const Gpu &gpu, Tables from, std::string &error) {
  SkyView = from.SkyView;
  Veil = from.Transmittance;
  Lut = from.Lut;
  if (SkyView == nullptr || Veil == nullptr || Lut == nullptr) {
    error = "the sky draw needs the sky view table and its sampler, and the plan did not hold both";
    return false;
  }

  const OwnedShader vertex(gpu.Device,
                           ShaderFrom(gpu.Device,
                                      "build/shaders/fullscreenNdc.vert.spv",
                                      SDL_GPU_SHADERSTAGE_VERTEX,
                                      ShaderShape,
                                      error));
  const OwnedShader fragment(gpu.Device,
                             ShaderFrom(gpu.Device,
                                        "build/shaders/sky.frag.spv",
                                        SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        ShaderShape,
                                        error));
  if (!vertex || !fragment) { return false; }

  std::array<SDL_GPUColorTargetDescription, kMaxColourAttachments> targets = {{}};
  size_t index = 0;
  for (const Resource resource : gpu.SceneColours) {
    auto &target = targets[index++];
    switch (resource) {
      case Resource::SceneHdr: target.format = gpu.HdrFormat; break;
      case Resource::SceneVelocity: target = VelocityTarget(true); break;
      case Resource::SceneShadingNormal:
        target.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        target.blend_state.enable_color_write_mask = true;
        target.blend_state.color_write_mask = 0;
        break;
      case Resource::SceneSurfaceIdentity:
        target.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        target.blend_state.enable_color_write_mask = true;
        target.blend_state.color_write_mask = 0;
        break;
      default: error = Says::UnsupportedSkyAttachment; return false;
    }
  }
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = targets.data();
  pipeline.target_info.num_color_targets = static_cast<uint32_t>(gpu.SceneColours.Size());
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

void SkyStage::Declare(const Medium &medium, SkyStanding stands) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.SunDir[axis] = stands.SunDir[axis];
    Pushed_.WorldUp[axis] = stands.WorldUp[axis];
  }
  Pushed_.Illuminance = stands.IlluminanceLux;
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (stands.EyeHeightM > 0.0f ? stands.EyeHeightM : 0.0f) / kMPerKmF;
  Pushed_.BottomRadiusKm = medium.BottomRadiusKm;
  Pushed_.SunHalfAngleRad = kSunHalfAngleRad;
  Pushed_.Air = medium;
  Pushed_.TopRadiusKm = medium.TopRadiusKm;
  Declared_ = true;
}

void SkyStage::Eye(const Medium &medium, float eyeHeightM) {
  if (!Declared_) { return; }
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / kMPerKmF;
}

void SkyStage::SetBasis(const EyeBasis &eye) {
  const float tanHalfW = eye.TanHalfWidth;
  const float tanHalfH = eye.TanHalfHeight;
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.Right[axis] = eye.Right[axis];
    Pushed_.Up[axis] = eye.Up[axis];
    Pushed_.Fwd[axis] = eye.Forward[axis];
  }
  Pushed_.TanHalf[0] = tanHalfW;
  Pushed_.TanHalf[1] = tanHalfH;
}

void SkyStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  if (!Pipe || !Declared_ || into.Pass == nullptr) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  const std::array<SDL_GPUTextureSamplerBinding, 2> bound = {
      {{.texture = SkyView, .sampler = Lut}, {.texture = Veil, .sampler = Lut}}};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, bound.data(), 2);
  SDL_PushGPUFragmentUniformData(into.Commands, 0, &Pushed_, static_cast<uint32_t>(sizeof Pushed_));
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

}
