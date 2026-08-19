#include <cstdio>
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

/* THE FUSED FRAGMENT, AND IT IS ONE PASS AND TWO TARGETS (board:1413). The catalogue fuses the
 * temporal resolve into this stage -- *the resolve and the display transfer are one fragment* -- so
 * the resolved radiance is never written, read back and written again: it leaves here as `SceneLinear`
 * for the next frame to reproject, and as the display picture, in one write each.
 *
 * THE ORDER OF THE TWO OUTPUTS IS THE PASS'S ATTACHMENT ORDER, which is the catalogue's: the resolve
 * contributes `SceneLinear` and this stage `FrameTex`, and the resolve's row comes first. */
const char *kTemporalFullScreenMsl = R"(
struct VOut { float4 pos [[position]]; };
vertex VOut vs(uint i [[vertex_id]]) {
  float2 corner[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
  VOut o;
  o.pos = float4(corner[i], 0.0, 1.0);
  return o;
}

struct Resolved {
  float4 linear [[color(0)]];
  float4 display [[color(1)]];
};

struct Temporal {
  float2 jitterDelta;   /* pixels: what the velocity carries and the motion does not */
  float2 texel;         /* 1 / size, so the reprojection is in uv */
  float historyHeld;    /* 0 on the first frame of a run: the present is then the answer */
  float pad0, pad1, pad2;
};

fragment Resolved fs(VOut in [[stage_in]],
                     constant Temporal &u [[buffer(0)]],
                     texture2d<float> scene [[texture(0)]], sampler sceneSampler [[sampler(0)]],
                     depth2d<float> sceneDepth [[texture(1)]], sampler depthSampler [[sampler(1)]],
                     texture2d<float> history [[texture(2)]], sampler smooth [[sampler(2)]],
                     texture2d<float> velocity [[texture(3)]], sampler velSampler [[sampler(3)]]) {
  uint2 px = uint2(in.pos.xy);
  float4 here = scene.read(px);

  /* The neighbourhood, gathered once and used for both the box and the mean. Nine reads of a target
   * already in cache; the expensive half of this is the history fetch, and it is one. */
  float3 lowest = float3(1.0e30);
  float3 highest = float3(-1.0e30);
  float3 total = float3(0.0);
  /* THE NEIGHBOURHOOD IS CLAMPED TO THE TARGET AND IT USED TO WRAP (board:1413). `read()` takes an
   * unsigned coordinate, so a pixel on the left or top edge asked for `uint(-1)` -- 4 294 967 295 --
   * and a texture read out of range is undefined in MSL. What came back entered the box and the mean,
   * and an infinity there survives `clipTowards`: `largest` is infinite, the branch is taken, and
   * `centre + offset / largest` is `inf + inf/inf`, which is NaN. A NaN in the resolved target is the
   * next frame's history, so it spreads. The edge row simply repeats itself now, which is what a 3x3
   * box means at a boundary. */
  int2 limit = int2(int(scene.get_width()) - 1, int(scene.get_height()) - 1);
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      uint2 at = uint2(clamp(int2(px) + int2(dx, dy), int2(0), limit));
      float3 neighbour = rgbToYCoCg(scene.read(at).rgb);
      lowest = min(lowest, neighbour);
      highest = max(highest, neighbour);
      total += neighbour;
    }
  }
  float3 centre = total * (1.0 / 9.0);
  float3 extent = max(highest - centre, centre - lowest);

  /* THE VELOCITY WITH THE JITTER TAKEN BACK OUT. Both view-projections carried their own frame's
   * sub-pixel offset, so what the geometry pass wrote is the true motion plus the difference. */
  float2 motion = velocity.read(px).xy - u.jitterDelta * u.texel;
  float2 uv = (float2(px) + 0.5) * u.texel;
  float2 was = uv - motion;

  float3 kept = here.rgb;
  bool inside = was.x >= 0.0 && was.x <= 1.0 && was.y >= 0.0 && was.y <= 1.0;
  if (inside && u.historyHeld > 0.5) {
    float3 past = rgbToYCoCg(history.sample(smooth, was).rgb);
    kept = mix(yCoCgToRgb(clipTowards(past, centre, extent)), here.rgb, kCurrentWeight);
  }

  Resolved out;
  out.linear = float4(kept, here.a);
  out.display = displayed(out.linear, sceneDepth.read(px));
  return out;
}
)";

constexpr uint32_t kTonemapImages = 2;
constexpr uint32_t kTemporalImages = 4;

} // namespace

bool TonemapStage::Configure(const Gpu &gpu, SDL_GPUTexture *scene, SDL_GPUTexture *depth,
                             SDL_GPUSampler *exact, SDL_GPUTextureFormat linear,
                             const DisplayOptions &options, std::string &error) {
  Scene = scene;
  Depth = depth;
  Exact = exact;

  Temporal = options.Temporal;
  const std::string source = std::string(kMslPrelude) + DisplayMsl(options) +
                             (options.Temporal ? kTemporalFullScreenMsl : kFullScreenMsl);
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = options.Temporal ? kTemporalImages : kTonemapImages;
  wanted.num_uniform_buffers = options.Temporal ? 1u : 0u;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the display transfer did not compile: ") + SDL_GetError();
    return false;
  }

  /* TWO DESCRIPTIONS WHERE THE RESOLVE IS FUSED IN, in the pass's own attachment order: the resolved
   * radiance carries the scene's format and the picture carries the surface's. */
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
  if (Temporal) {
    if (History == nullptr || Velocity == nullptr) { return; }
    struct {
      float JitterDelta[2];
      float Texel[2];
      float HistoryHeld;
      float Pad[3];
    } uniforms{{JitterDelta[0], JitterDelta[1]},
               {Width > 0 ? 1.0f / (float)Width : 0.0f, Height > 0 ? 1.0f / (float)Height : 0.0f},
               HistoryHeld ? 1.0f : 0.0f,
               {0.0f, 0.0f, 0.0f}};
    SDL_PushGPUFragmentUniformData(into.Commands, 0, &uniforms, sizeof uniforms);
    const SDL_GPUTextureSamplerBinding images[kTemporalImages] = {
        {Scene, Exact}, {Depth, Exact}, {History, Exact}, {Velocity, Exact}};
    SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kTemporalImages);
  } else {
    const SDL_GPUTextureSamplerBinding images[kTonemapImages] = {{Scene, Exact}, {Depth, Exact}};
    SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kTonemapImages);
  }
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

} // namespace outshine::Render
