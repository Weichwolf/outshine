#include "SkyStage.h"

#include <numbers>
#include <cstdio>

#include "SceneTargets.h"
#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {

}

bool SkyStage::Configure(const Gpu &gpu, SDL_GPUTexture *skyView, SDL_GPUSampler *lut,
                         std::string &error) {
  SkyView = skyView;
  Lut = lut;
  if (SkyView == nullptr || Lut == nullptr) {
    error = "the sky draw needs the sky view table and its sampler, and the plan did not hold both";
    return false;
  }
  if (Pipe) { return true; }

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
  if (!made) {
    error = std::string("the sky's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void SkyStage::Declare(const Medium &medium, const float sunDir[3], const float up[3],
                       float illuminanceLux, float eyeHeightM) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.SunDir[axis] = sunDir[axis];
    Pushed_.WorldUp[axis] = up[axis];
  }
  Pushed_.Illuminance = illuminanceLux;
  Pushed_.EyeRadiusKm =
      medium.BottomRadiusKm + kMediumGroundLiftKm + (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / 1000.0f;
  Pushed_.BottomRadiusKm = medium.BottomRadiusKm;
  Pushed_.TopRadiusKm = medium.TopRadiusKm;
  Declared_ = true;
}

void SkyStage::SetBasis(const float right[3], const float upAxis[3], const float fwd[3],
                        float tanHalfW, float tanHalfH) {
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
  SDL_GPUTextureSamplerBinding bound{SkyView, Lut};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, &bound, 1);
  SDL_PushGPUFragmentUniformData(into.Commands, 0, &Pushed_, (uint32_t)sizeof Pushed_);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

std::string SkyStage::ShaderSource() {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string SkyStage::ShaderSource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/sky.msl", body, error)) { return std::string(); }
  return MslPrelude() + VelocityStaticDefine() + body;
}

}
