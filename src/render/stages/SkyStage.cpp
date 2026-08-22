#include "SkyStage.h"

#include <numbers>
#include <cstdio>

#include "SceneTargets.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {

const char *kSkyMsl = R"(
struct Pushed {
  float4 right;
  float4 up;
  float4 fwd;
  float4 worldUp;
  float4 sunDir;
  float2 tanHalf;
  float illuminance;
  float eyeRadiusKm;
  float bottomRadiusKm;
  float topRadiusKm;
  float2 pad;
};

struct VOut { float4 pos [[position]]; float2 ndc; };
vertex VOut vs(uint i [[vertex_id]]) {
  float2 corner[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
  VOut o;
  o.pos = float4(corner[i], 0.0, 1.0);
  o.ndc = corner[i];
  return o;
}

struct SkyOut {
  float4 colour [[color(0)]];
  float2 velocity [[color(1)]];
};

fragment SkyOut fs(VOut in [[stage_in]],
                   texture2d<float> skyView [[texture(0)]],
                   sampler lut [[sampler(0)]],
                   constant Pushed &pushed [[buffer(0)]]) {
  float3 dir = normalize(pushed.fwd.xyz + in.ndc.x * pushed.tanHalf.x * pushed.right.xyz +
                         in.ndc.y * pushed.tanHalf.y * pushed.up.xyz);
  float3 worldUp = pushed.worldUp.xyz;
  float cosView = dot(dir, worldUp);

  float3 side = cross(worldUp, dir);
  float sideLength = length(side);
  float lightViewCos = 1.0;
  if (sideLength > 1.0e-5) {
    side = side / sideLength;
    float3 forward = normalize(cross(side, worldUp));
    float2 lightOnPlane =
        normalize(float2(dot(pushed.sunDir.xyz, forward), dot(pushed.sunDir.xyz, side)));
    lightViewCos = lightOnPlane.x;
  }

  float radiusKm = pushed.eyeRadiusKm;
  float toHorizon = sqrt(max(0.0, radiusKm * radiusKm - pushed.bottomRadiusKm * pushed.bottomRadiusKm));
  float beta = acos(clamp(toHorizon / radiusKm, -1.0, 1.0));
  float zenithToHorizon = 3.14159265358979 - beta;
  bool hitsGround = acos(clamp(cosView, -1.0, 1.0)) > zenithToHorizon;

  float widthPx = float(skyView.get_width());
  float heightPx = float(skyView.get_height());
  float v;
  if (!hitsGround) {
    float coord = acos(clamp(cosView, -1.0, 1.0)) / zenithToHorizon;
    coord = 1.0 - sqrt(max(0.0, 1.0 - coord));
    v = coord * 0.5;
  } else {
    float coord = (acos(clamp(cosView, -1.0, 1.0)) - zenithToHorizon) / beta;
    v = sqrt(max(0.0, coord)) * 0.5 + 0.5;
  }
  float u = sqrt(max(0.0, -lightViewCos * 0.5 + 0.5));
  u = (u + 0.5 / widthPx) * (widthPx / (widthPx + 1.0));
  v = (v + 0.5 / heightPx) * (heightPx / (heightPx + 1.0));

  float3 luminance = skyView.sample(lut, float2(u, v), level(0.0)).rgb * pushed.illuminance;
  SkyOut out;
  out.colour = float4(luminance, 1.0);
  out.velocity = float2(VELOCITY_STATIC, VELOCITY_STATIC);
  return out;
}
)";

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

  char constants[128];
  std::snprintf(constants, sizeof constants, "#define VELOCITY_STATIC %.9ef\n",
                (double)kVelocityStatic);
  const std::string source = std::string(kMslPrelude) + constants + kSkyMsl;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = 1u;
  wanted.num_uniform_buffers = 1u;
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

}
