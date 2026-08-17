/* WHAT THE SAMPLER RETURNS FROM EACH LEVEL OF A CHAIN THIS ENGINE UPLOADED (board:1130).
 *
 * WHY THIS EXISTS. `board:1130` set `min_lod`/`max_lod` so the chain became reachable and six appearance
 * tails saturated -- `ours 255 against 0` at a single pixel, on OPAQUE materials. Seam LOD has since
 * been eliminated on the device, and four arithmetic paths between a reachable chain and a bright pixel
 * were read and all carry a bound. **What no reading settles is whether the levels this engine uploads
 * read back as the bytes it put there**, and that is the one remaining step between the chain and the
 * pixel.
 *
 * THE SUSPICION IS THE SMALL LEVELS AND IT IS SPECIFIC. `SubjectDraw` uploads every level itself, one
 * copy pass each, with `pixels_per_row = levelWidth` -- and the last levels of any chain are 4x4, 2x2
 * and 1x1. A row pitch an API rounds up, or a transfer sized for texels rather than for a required
 * alignment, reads adjacent memory: garbage that is bounded by nothing and lands full-bright as often
 * as not. **A level that is never sampled cannot show it**, which is exactly the state `max_lod = 0`
 * held the engine in.
 *
 * THE INSTRUMENT IS THE CHAIN ITSELF AND NOTHING AROUND IT. Each level is filled with a DISTINCT
 * constant, the fragment samples an explicitly named level, and the returned value is checked against
 * the constant that level was given. No asset, no camera, no oracle, no corpus -- and no LOD selection
 * either, because `level()` states which one rather than deriving it. */
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

/* 64 -> 1 is seven levels, which reaches the 1x1 the suspicion is about while staying small enough that
 * the whole chain is uploaded in well under a millisecond. The raster is one row per level. */
constexpr uint32_t kMapSide = 64;
constexpr uint32_t kLevels = 7;
constexpr uint32_t kSide = 64;

/* WHAT LEVEL `n` IS FILLED WITH, and the values are distinct and none is 0 or 1: a level read back as
 * either would be indistinguishable from a clear or from a saturation, which is the failure being
 * looked for. */
float FillOf(uint32_t level) { return 0.1f + 0.1f * (float)level; }

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

std::string ChainShader() {
  return std::string(kMslPrelude) + R"(
struct Carried { float4 pos [[position]]; float2 uv; };

vertex Carried across(uint vid [[vertex_id]]) {
  const float2 corner[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
  Carried out;
  out.pos = float4(corner[vid], 0.0, 1.0);
  /* `uv.y` COUNTS DOWN FROM THE TOP, THE WAY THE READBACK'S ROWS DO. Clip y = -1 is the BOTTOM of the
   * screen and row 0 of the read image is the TOP, so the obvious `corner * 0.5 + 0.5` maps level 0
   * onto the last row -- which this probe reported as every level fetching another level's fill,
   * reversed, with the middle one exact. The values were a permutation of the uploaded set, which is
   * what said the chain was right and the reader was not. */
  out.uv = float2(corner[vid].x * 0.5 + 0.5, 0.5 - corner[vid].y * 0.5);
  return out;
}

/* THE ROW DECIDES THE LEVEL, so one pass reads every level of the chain and the comparison is between
 * rows of one image rather than between runs. `level()` names it outright: no derivative, no selection,
 * nothing between the declaration and the fetch. */
fragment float4 fetched(Carried in [[stage_in]], texture2d<float> map [[texture(0)]],
                        sampler smp [[sampler(0)]], constant uint &levels [[buffer(0)]]) {
  uint row = uint(clamp(in.uv.y, 0.0, 0.999) * float(levels));
  float4 texel = map.sample(smp, float2(0.5, 0.5), level(float(row)));
  return float4(texel.r, texel.g, texel.b, 1.0);
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

/* THE UPLOAD IS THE SUBJECT, so it is written the way `SubjectDraw` writes it: one transfer buffer and
 * one copy pass per level, `pixels_per_row` set to that level's own width. A probe that uploaded the
 * chain some other way would be testing a path this engine does not take. */
bool FillChain(SDL_GPUDevice *device, SDL_GPUTexture *map) {
  uint32_t width = kMapSide, height = kMapSide;
  for (uint32_t level = 0; level < kLevels; ++level) {
    const uint32_t bytes = width * height * 4u * (uint32_t)sizeof(float);
    std::vector<float> texels((size_t)width * height * 4u, FillOf(level));
    SDL_GPUTransferBufferCreateInfo wantedTransfer{};
    wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    wantedTransfer.size = bytes;
    SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(device, &wantedTransfer);
    if (Refused(staging, "a transfer buffer")) { return false; }
    std::memcpy(SDL_MapGPUTransferBuffer(device, staging, false), texels.data(), bytes);
    SDL_UnmapGPUTransferBuffer(device, staging);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging;
    source.pixels_per_row = width;
    source.rows_per_layer = height;
    SDL_GPUTextureRegion into{};
    into.texture = map;
    into.mip_level = level;
    into.w = width;
    into.h = height;
    into.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &into, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, staging);
    width = width > 1u ? width / 2u : 1u;
    height = height > 1u ? height / 2u : 1u;
  }
  return true;
}

struct Rendered {
  std::vector<float> Rgba;
  bool Ready = false;
};

Rendered Raster(const Instrument &on) {
  Rendered out;
  const std::string source = ChainShader();
  SDL_GPUShader *vertex = Stage(on.Device, source, "across", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
  if (Refused(vertex, "the vertex stage")) { return out; }
  SDL_GPUShader *fragment = Stage(on.Device, source, "fetched", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
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

  SDL_GPUTextureCreateInfo wantedMap{};
  wantedMap.type = SDL_GPU_TEXTURETYPE_2D;
  wantedMap.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  wantedMap.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedMap.width = kMapSide;
  wantedMap.height = kMapSide;
  wantedMap.layer_count_or_depth = 1;
  wantedMap.num_levels = kLevels;
  wantedMap.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture *map = SDL_CreateGPUTexture(on.Device, &wantedMap);

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.min_filter = SDL_GPU_FILTER_LINEAR;
  wantedSampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  wantedSampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  wantedSampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.max_lod = (float)(kLevels - 1u);
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
      Refused(sampler, "the sampler") || Refused(pipeline, "the pipeline") ||
      !FillChain(on.Device, map)) {
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
    const uint32_t levels = kLevels;
    SDL_PushGPUFragmentUniformData(commands, 0, &levels, sizeof levels);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
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

  const Rendered got = Raster(on);
  CHECK(DeviceErrors == 0, "the device accepted every object this probe declared");
  CHECK(got.Ready, "the chain rasterised and read back");
  if (!got.Ready) { return Report(); }

  /* THE ROW A LEVEL OWNS is `[level, level + 1) * kSide / kLevels`, and the middle of it is sampled so
   * the reading is never on a boundary the row arithmetic could round either way. */
  size_t wrong = 0;
  double worst = 0;
  for (uint32_t level = 0; level < kLevels; ++level) {
    const uint32_t row =
        (uint32_t)(((double)level + 0.5) * (double)kSide / (double)kLevels);
    const size_t at = ((size_t)row * kSide + kSide / 2u) * 4u;
    const double want = FillOf(level);
    const double red = got.Rgba[at];
    const double apart = std::fabs(red - want);
    worst = std::max(worst, apart);
    if (apart > 1e-5) { ++wrong; }
    std::printf("NOTE level %u fetched %.9g, uploaded %.9g, apart %.3e\n", level, red, want, apart);
  }

  Note("levels whose fetch disagrees with what was uploaded", (double)wrong, "levels");
  Note("worst disagreement over the chain", worst, "linear");

  CHECK(wrong == 0,
        "every level of a chain this engine uploaded reads back as the value it was given -- so the "
        "upload's per-level row pitch is right down to the 1x1, and a saturating pixel under a "
        "reachable chain is not this");

  Covers("I.26 every level of an uploaded mip chain reads back as itself, including the small levels "
         "a row pitch would corrupt");
  return Report();
}
