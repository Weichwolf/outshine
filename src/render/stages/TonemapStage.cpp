#include "TonemapStage.h"

#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

/* One triangle covering the frame, so there is no vertex buffer and no index buffer to own. */
const char *kFullScreenMsl = R"(
struct VOut { float4 pos [[position]]; };
vertex VOut vs(uint i [[vertex_id]]) {
  float2 corner[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
  VOut o;
  o.pos = float4(corner[i], 0.0, 1.0);
  return o;
}
fragment float4 fs(VOut in [[stage_in]],
                   texture2d<float> scene [[texture(0)]], sampler sceneSampler [[sampler(0)]],
                   depth2d<float> sceneDepth [[texture(1)]], sampler depthSampler [[sampler(1)]]) {
  uint2 px = uint2(in.pos.xy);
  return displayed(scene.read(px), sceneDepth.read(px));
}
)";

constexpr uint32_t kTonemapImages = 2;

} // namespace

bool TonemapStage::Configure(const Gpu &gpu, SDL_GPUTexture *scene, SDL_GPUTexture *depth,
                             SDL_GPUSampler *exact, const DisplayOptions &options,
                             std::string &error) {
  Scene = scene;
  Depth = depth;
  Exact = exact;

  const std::string source = std::string(kMslPrelude) + DisplayMsl(options) + kFullScreenMsl;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = kTonemapImages;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the display transfer did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUColorTargetDescription target{};
  target.format = gpu.SurfaceFormat;
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
    error = std::string("the display transfer's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void TonemapStage::Encode(const FrameContext &, const PassRecording &into) {
  if (!Pipe) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  const SDL_GPUTextureSamplerBinding images[kTonemapImages] = {{Scene, Exact}, {Depth, Exact}};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kTonemapImages);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

} // namespace outshine::Render
