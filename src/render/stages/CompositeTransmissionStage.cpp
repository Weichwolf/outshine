#include "CompositeTransmissionStage.h"

#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

/* One triangle covering the frame, so there is no vertex buffer and no index buffer to own.
 *
 * THE ARITHMETIC IS PREMULTIPLIED `over` AND NOTHING ELSE. The glass pass writes the radiance it
 * emits ALREADY MULTIPLIED by its own coverage, and that coverage in alpha; so what is behind it is
 * attenuated by `1 - a` and the two are added. Writing it any other way -- a lerp on the colour, a
 * second multiply here -- would apply coverage twice wherever two transmissive fragments overlap. */
const char *kCompositeMsl = R"(
struct VOut { float4 pos [[position]]; };
vertex VOut vs(uint i [[vertex_id]]) {
  float2 corner[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
  VOut o;
  o.pos = float4(corner[i], 0.0, 1.0);
  return o;
}
fragment float4 fs(VOut in [[stage_in]],
                   texture2d<float> opaque [[texture(0)]], sampler opaqueSampler [[sampler(0)]],
                   texture2d<float> glass [[texture(1)]], sampler glassSampler [[sampler(1)]]) {
  uint2 px = uint2(in.pos.xy);
  float4 behind = opaque.read(px);
  float4 front = glass.read(px);
  return float4(behind.rgb * (1.0 - front.a) + front.rgb, behind.a);
}
)";

constexpr uint32_t kCompositeImages = 2;

} // namespace

bool CompositeTransmissionStage::Configure(const Gpu &gpu, SDL_GPUTexture *opaque,
                                           SDL_GPUTexture *transmissive, SDL_GPUSampler *exact,
                                           std::string &error) {
  Opaque = opaque;
  Transmissive = transmissive;
  Exact = exact;

  const std::string source = std::string(kMslPrelude) + kCompositeMsl;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = kCompositeImages;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the transmission composite did not compile: ") + SDL_GetError();
    return false;
  }

  /* THE TARGET IS THE SCENE'S OWN FORMAT AND NOT THE SURFACE'S: this stage stays in linear radiance
   * and the display transfer is still the only place a frame leaves it. */
  SDL_GPUColorTargetDescription target{};
  target.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
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

} // namespace outshine::Render
