#include "TemporalResolveStage.h"

#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

/* THE CLAMP IS IN YCoCg AND THAT IS THE ONE CHOICE HERE WORTH DEFENDING. The history is clipped into
 * the axis-aligned box of the current frame's 3x3 neighbourhood; in RGB that box is a poor fit to the
 * set of colours a pixel could plausibly be, so an edge between two hues clips towards a colour
 * NEITHER neighbour has and the picture gains fringes. In a luma-chroma basis the box is aligned with
 * the axis the eye is sensitive to and the same clip keeps the hue -- Karis, *High Quality Temporal
 * Supersampling*, SIGGRAPH 2014, and Salvi's *An Excursion in Temporal Supersampling*, GDC 2016.
 *
 * IT IS A CLIP TOWARDS THE MEAN AND NOT A CLAMP PER CHANNEL. Clamping each channel independently
 * lands on a corner of the box and shifts the colour; scaling the whole offset until it enters the
 * box keeps the direction the history disagreed in, which is what makes disocclusion fade rather than
 * flash.
 *
 * THE BLEND WEIGHT IS DECLARED AND IT IS THE ONE NUMBER A READER SHOULD SEE. `kCurrentWeight` is the
 * fraction of the present in each output pixel, so the history's half-life is
 * `-1 / log2(1 - kCurrentWeight)` frames -- at 0.1 that is 6.6 frames, which at 60 Hz is 110 ms of
 * accumulation and is the trade between shimmer and smear. */
const char *kTemporalMsl = R"(
constant float kCurrentWeight = 0.1;

struct VOut { float4 pos [[position]]; };

vertex VOut vs(uint i [[vertex_id]]) {
  float2 corner[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
  VOut o;
  o.pos = float4(corner[i], 0.0, 1.0);
  return o;
}

static inline float3 rgbToYCoCg(float3 c) {
  return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
                0.5 * c.r - 0.5 * c.b,
                -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

static inline float3 yCoCgToRgb(float3 c) {
  float t = c.x - c.z;
  return float3(t + c.y, c.x + c.z, t - c.y);
}

/* Scale the offset from the box's centre until it is inside. A component whose extent is zero cannot
 * constrain anything, so it is skipped rather than made into a division. */
static inline float3 clipTowards(float3 history, float3 centre, float3 extent) {
  float3 offset = history - centre;
  float3 unit = abs(offset) / max(extent, float3(1.0e-5));
  float largest = max(max(unit.x, unit.y), unit.z);
  return largest > 1.0 ? centre + offset / largest : history;
}

struct Uniforms {
  float2 jitterDelta;   /* pixels: what the velocity carries and the motion does not */
  float2 texel;         /* 1 / size, so the neighbourhood walk is in uv */
  float historyHeld;    /* 0 on the first frame of a run, and then the present is the answer */
  float pad0, pad1, pad2;
};

fragment float4 fs(VOut in [[stage_in]],
                   constant Uniforms &u [[buffer(0)]],
                   texture2d<float> current [[texture(0)]], sampler exact [[sampler(0)]],
                   texture2d<float> history [[texture(1)]], sampler smooth [[sampler(1)]],
                   texture2d<float> velocity [[texture(2)]], sampler exactVel [[sampler(2)]]) {
  uint2 px = uint2(in.pos.xy);
  float4 here = current.read(px);

  /* THE NEIGHBOURHOOD, GATHERED ONCE AND USED FOR BOTH THE BOX AND THE MEAN. Nine reads of a target
   * already in cache, which is the cheap half of this stage; the expensive half is the history
   * fetch, and it is one. */
  float3 lowest = float3(1.0e30);
  float3 highest = float3(-1.0e30);
  float3 total = float3(0.0);
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      uint2 at = uint2(int2(px) + int2(dx, dy));
      float3 neighbour = rgbToYCoCg(current.read(at).rgb);
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

  /* A REPROJECTION THAT LEFT THE FRAME HAS NO PAST, and neither does the first frame of a run. Both
   * answer with the present rather than with whatever the texture held. */
  bool inside = was.x >= 0.0 && was.x <= 1.0 && was.y >= 0.0 && was.y <= 1.0;
  if (!inside || u.historyHeld < 0.5) { return here; }

  float3 past = rgbToYCoCg(history.sample(smooth, was).rgb);
  float3 kept = yCoCgToRgb(clipTowards(past, centre, extent));
  return float4(mix(kept, here.rgb, kCurrentWeight), here.a);
}
)";

constexpr uint32_t kTemporalImages = 3;

struct Uniforms {
  float JitterDelta[2];
  float Texel[2];
  float HistoryHeld;
  float Pad[3];
};

} // namespace

bool TemporalResolveStage::Configure(const Gpu &gpu, SDL_GPUTextureFormat target,
                                     SDL_GPUSampler *exact, SDL_GPUSampler *smooth,
                                     std::string &error) {
  Exact = exact;
  Smooth = smooth;

  const std::string source = std::string(kMslPrelude) + kTemporalMsl;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = kTemporalImages;
  wanted.num_uniform_buffers = 1;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the temporal resolve did not compile: ") + SDL_GetError();
    return false;
  }

  /* THE TARGET IS THE SCENE'S OWN FORMAT, HANDED IN FROM THE PLAN: this stage stays in linear
   * radiance and the display transfer is still the only place a frame leaves it. */
  SDL_GPUColorTargetDescription description{};
  description.format = target;
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = &description;
  pipeline.target_info.num_color_targets = 1;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (!made) {
    error = std::string("the temporal resolve's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);
  return true;
}

void TemporalResolveStage::Bind(SDL_GPUTexture *current, SDL_GPUTexture *history,
                                SDL_GPUTexture *velocity, int width, int height) {
  Current = current;
  History = history;
  Velocity = velocity;
  Width = width;
  Height = height;
}

void TemporalResolveStage::Encode(const PassRecording &into, const float jitterDelta[2],
                                  bool historyHeld) {
  if (!Pipe || Current == nullptr || History == nullptr || Velocity == nullptr) { return; }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  Uniforms uniforms{};
  uniforms.JitterDelta[0] = jitterDelta[0];
  uniforms.JitterDelta[1] = jitterDelta[1];
  uniforms.Texel[0] = Width > 0 ? 1.0f / (float)Width : 0.0f;
  uniforms.Texel[1] = Height > 0 ? 1.0f / (float)Height : 0.0f;
  uniforms.HistoryHeld = historyHeld ? 1.0f : 0.0f;
  SDL_PushGPUFragmentUniformData(into.Commands, 0, &uniforms, sizeof uniforms);
  const SDL_GPUTextureSamplerBinding images[kTemporalImages] = {
      {Current, Exact}, {History, Smooth}, {Velocity, Exact}};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kTemporalImages);
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
}

} // namespace outshine::Render
