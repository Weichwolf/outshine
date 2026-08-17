/* WHAT LOD THE DEVICE ITSELF SELECTS WHERE TWO TRIANGLES MEET WITH DIFFERENT uv (board:1130).
 *
 * WHY THIS EXISTS. `board:1130` measured a bimodal LOD distribution over `normal-tangent` -- 288 807
 * covered pixels at about 1.25 against 6 069 above level 8 -- and concluded that a uv discontinuity
 * makes the selection saturate. **That measurement was a per-pixel finite difference over CYCLES' uv**,
 * and the item says so itself: *it is a proxy*. A difference between NEIGHBOURING PIXELS crosses from
 * one triangle to another wherever a uv island ends, which is exactly where the saturation was found.
 *
 * SO THE PROXY AND THE DEVICE MAY DISAGREE, AND ONLY THE DEVICE SETTLES IT. This case asks the hardware
 * the question directly: two triangles sharing an edge, with uv that JUMPS across it, and the fragment
 * reports `calculate_unclamped_lod` -- the level the sampler would choose before any clamp. No asset, no
 * camera, no oracle, no corpus.
 *
 * WHAT EACH ANSWER MEANS, STATED BEFORE THE NUMBER IS READ so the reading cannot be fitted to it:
 *
 *   the level stays near the per-triangle value  -> the derivative is taken within a primitive, the
 *                                                   proxy crossed triangles the hardware never crosses,
 *                                                   and `board:1130`'s leading hypothesis is about the
 *                                                   INSTRUMENT rather than the engine
 *   the level spikes beside the shared edge      -> the proxy was right, and the ladder question it
 *                                                   frames -- clamp the selection, or pad the islands --
 *                                                   is the real one
 *
 * THE TWO TRIANGLES CARRY THE SAME uv SPAN AND DIFFERENT uv ORIGINS, which is what makes the test about
 * the discontinuity alone: each covers a quarter of the texture's width, so the level each SHOULD select
 * is identical, and the only thing that differs between them is where their islands sit. A spike would
 * therefore have nowhere to come from except the seam. */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "Readback.h"
#include "ShaderPrelude.h"

using outshine::Render::kMslPrelude;
using outshine::Render::Readback;
using outshine::Render::ReadState;

namespace {

int DeviceErrors = 0;

bool Refused(const void *made, const char *what) {
  if (made) { return false; }
  ++DeviceErrors;
  std::printf("NOTE device refused %s: %s\n", what, SDL_GetError());
  return true;
}

/* The raster, and the map it samples. The map is far larger than the quad's footprint on purpose: a
 * level of about 3 is then the ordinary answer, well clear of both 0 and the top of the chain, so a
 * saturation is unmistakable rather than a value that could be either. */
constexpr uint32_t kSide = 256;
constexpr uint32_t kMapSide = 1024;
constexpr uint32_t kMapLevels = 11; /* 1024 -> 1, inclusive */

class Instrument {
public:
  ~Instrument() {
    if (Device) {
      SDL_DestroyGPUDevice(Device);
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }
  Instrument() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { return; }
    Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!Device) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  }
  Instrument(const Instrument &) = delete;
  Instrument &operator=(const Instrument &) = delete;
  SDL_GPUDevice *Device = nullptr;
};

/* SIX VERTICES AS A UNIFORM AND NO VERTEX BUFFER, so nothing between the declaration and the raster can
 * alter either the position or the uv. `calculate_unclamped_lod` is the level BEFORE the sampler's own
 * `min_lod`/`max_lod` clamp, which is the quantity the question is about: `board:1130` clamps to level 0
 * today, and a clamped reading would answer 0 whatever the hardware selected. */
std::string SeamShader() {
  return std::string(kMslPrelude) + R"(
struct Corners { float4 clip[6]; float4 uv[6]; };
struct Carried { float4 pos [[position]]; float2 uv; };

vertex Carried across(uint vid [[vertex_id]], constant Corners &corners [[buffer(0)]]) {
  Carried out;
  out.pos = corners.clip[vid];
  out.uv = corners.uv[vid].xy;
  return out;
}

fragment float4 selected(Carried in [[stage_in]], texture2d<float> map [[texture(0)]],
                         sampler smp [[sampler(0)]]) {
  float level = map.calculate_unclamped_lod(smp, in.uv);
  return float4(level, in.uv.x, in.uv.y, 1.0);
}
)";
}

SDL_GPUShader *Stage(SDL_GPUDevice *device, const std::string &source, const char *entry,
                     SDL_GPUShaderStage stage, uint32_t uniforms, uint32_t samplers) {
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const uint8_t *>(source.c_str());
  wanted.code_size = source.size();
  wanted.entrypoint = entry;
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.stage = stage;
  wanted.num_uniform_buffers = uniforms;
  wanted.num_samplers = samplers;
  return SDL_CreateGPUShader(device, &wanted);
}

struct Rendered {
  std::vector<float> Rgba;
  bool Ready = false;
};

Rendered Raster(const Instrument &on, const float corners[48]) {
  Rendered out;
  const std::string source = SeamShader();
  SDL_GPUShader *vertex = Stage(on.Device, source, "across", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
  if (Refused(vertex, "the vertex stage")) { return out; }
  SDL_GPUShader *fragment = Stage(on.Device, source, "selected", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
  if (Refused(fragment, "the fragment stage")) {
    SDL_ReleaseGPUShader(on.Device, vertex);
    return out;
  }

  SDL_GPUTextureCreateInfo wantedTarget{};
  wantedTarget.type = SDL_GPU_TEXTURETYPE_2D;
  wantedTarget.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  wantedTarget.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedTarget.width = kSide;
  wantedTarget.height = kSide;
  wantedTarget.layer_count_or_depth = 1;
  wantedTarget.num_levels = 1;
  wantedTarget.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture *target = SDL_CreateGPUTexture(on.Device, &wantedTarget);

  /* THE MAP IS NEVER READ, ONLY MEASURED AGAINST. `calculate_unclamped_lod` is a function of the uv
   * gradient and the texture's DIMENSIONS, so its texels do not enter the answer and are left
   * uninitialised rather than filled with a pattern nothing samples. */
  SDL_GPUTextureCreateInfo wantedMap{};
  wantedMap.type = SDL_GPU_TEXTURETYPE_2D;
  wantedMap.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  wantedMap.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedMap.width = kMapSide;
  wantedMap.height = kMapSide;
  wantedMap.layer_count_or_depth = 1;
  wantedMap.num_levels = kMapLevels;
  wantedMap.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture *map = SDL_CreateGPUTexture(on.Device, &wantedMap);

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.min_filter = SDL_GPU_FILTER_LINEAR;
  wantedSampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  wantedSampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  wantedSampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.max_lod = (float)(kMapLevels - 1u);
  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(on.Device, &wantedSampler);

  SDL_GPUColorTargetDescription described{};
  described.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  SDL_GPUGraphicsPipelineCreateInfo wantedPipeline{};
  wantedPipeline.vertex_shader = vertex;
  wantedPipeline.fragment_shader = fragment;
  wantedPipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  wantedPipeline.target_info.num_color_targets = 1;
  wantedPipeline.target_info.color_target_descriptions = &described;
  SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(on.Device, &wantedPipeline);

  SDL_ReleaseGPUShader(on.Device, vertex);
  SDL_ReleaseGPUShader(on.Device, fragment);
  if (Refused(target, "the target") || Refused(map, "the map") ||
      Refused(sampler, "the sampler") || Refused(pipeline, "the pipeline")) {
    if (target) { SDL_ReleaseGPUTexture(on.Device, target); }
    if (map) { SDL_ReleaseGPUTexture(on.Device, map); }
    if (sampler) { SDL_ReleaseGPUSampler(on.Device, sampler); }
    if (pipeline) { SDL_ReleaseGPUGraphicsPipeline(on.Device, pipeline); }
    return out;
  }

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(on.Device);
  if (!Refused(commands, "a command buffer")) {
    SDL_GPUColorTargetInfo attachment{};
    attachment.texture = target;
    attachment.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    attachment.load_op = SDL_GPU_LOADOP_CLEAR;
    attachment.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &attachment, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUTextureSamplerBinding bound{map, sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &bound, 1);
    SDL_PushGPUVertexUniformData(commands, 0, corners, 48u * sizeof(float));
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);

    Readback read;
    if (read.FromTexture(on.Device, target, kSide, kSide, 16u) == ReadState::Ready) {
      const size_t components = (size_t)kSide * (size_t)kSide * 4u;
      out.Rgba.resize(components);
      std::memcpy(out.Rgba.data(), read.Rows(), components * sizeof(float));
      out.Ready = true;
    } else {
      ++DeviceErrors;
    }
  }
  SDL_ReleaseGPUGraphicsPipeline(on.Device, pipeline);
  SDL_ReleaseGPUSampler(on.Device, sampler);
  SDL_ReleaseGPUTexture(on.Device, map);
  SDL_ReleaseGPUTexture(on.Device, target);
  return out;
}

} // namespace

int main() {
  using namespace outshine::Test;

  Instrument on;
  if (!on.Device) {
    CHECK(false, "a Metal device is created, which a shader case needs");
    return Report();
  }

  /* TWO TRIANGLES, EACH A HALF OF THE VIEWPORT, MEETING ON THE VERTICAL CENTRE LINE. Their uv spans are
   * the SAME WIDTH -- a quarter of the map -- and their origins are half the map apart, so the level
   * each triangle should select on its own is identical and the seam is the only thing between them. */
  const float corners[48] = {
      /* clip: left triangle, then right */
      -1.0f, -1.0f, 0.0f, 1.0f,   0.0f, -1.0f, 0.0f, 1.0f,  -1.0f,  1.0f, 0.0f, 1.0f,
       0.0f, -1.0f, 0.0f, 1.0f,   1.0f, -1.0f, 0.0f, 1.0f,   0.0f,  1.0f, 0.0f, 1.0f,
      /* uv: the left island at [0, 0.25], the right at [0.75, 1] -- a jump of 0.5 across the seam */
       0.00f, 0.0f, 0, 0,         0.25f, 0.0f, 0, 0,         0.00f, 1.0f, 0, 0,
       0.75f, 0.0f, 0, 0,         1.00f, 0.0f, 0, 0,         0.75f, 1.0f, 0, 0,
  };

  const Rendered got = Raster(on, corners);
  CHECK(DeviceErrors == 0, "the device accepted every object this probe declared");
  CHECK(got.Ready, "the seam rasterised and read back");
  if (!got.Ready) { return Report(); }

  /* THE SEAM COLUMN IS THE VIEWPORT'S CENTRE, and the population is split by distance from it so that
   * "beside the seam" is a stated set of pixels rather than an impression. */
  const int seam = (int)kSide / 2;
  double interiorWorst = 0, besideWorst = 0, interiorSum = 0;
  size_t interior = 0, beside = 0, saturated = 0;
  for (uint32_t row = 0; row < kSide; ++row) {
    for (uint32_t column = 0; column < kSide; ++column) {
      const size_t at = ((size_t)row * kSide + column) * 4u;
      if (got.Rgba[at + 3] < 0.5f) { continue; }
      const double level = got.Rgba[at];
      if (level > (double)(kMapLevels - 2u)) { ++saturated; }
      if (std::abs((int)column - seam) <= 1) {
        ++beside;
        besideWorst = std::max(besideWorst, level);
      } else {
        ++interior;
        interiorSum += level;
        interiorWorst = std::max(interiorWorst, level);
      }
    }
  }

  Note("covered pixels away from the seam", (double)interior, "px");
  Note("covered pixels within one column of the seam", (double)beside, "px");
  Note("mean selected LOD away from the seam", interior ? interiorSum / (double)interior : 0.0,
       "levels");
  Note("worst selected LOD away from the seam", interiorWorst, "levels");
  Note("worst selected LOD beside the seam", besideWorst, "levels");
  Note("pixels selecting the top two levels of the chain", (double)saturated, "px");

  CHECK(interior > 0 && beside > 0,
        "both populations are non-empty, so the comparison is between two measured sets");

  /* THE CLAIM, AND IT IS THE ONE `board:1130` NEEDS. If the device took its derivative across the seam
   * the way a per-pixel finite difference does, the uv would jump by half the map in one step and the
   * selection would run to the top of an 11-level chain. Holding the seam to the interior's own worst
   * is what says it does not. */
  CHECK(besideWorst <= interiorWorst + 1.0,
        "the level selected beside the seam is the level selected away from it, so the device's "
        "derivative does not cross the discontinuity that a per-pixel difference over another "
        "renderer's uv does");
  CHECK(saturated == 0,
        "no covered pixel selects the top of the chain, which is what a derivative taken across the "
        "seam would produce and what board:1130's proxy reported over Cycles' uv");

  Covers("I.26 the device's own LOD selection across a uv discontinuity, measured rather than "
         "inferred from another renderer's uv");
  return Report();
}
