/* HOW MANY POSITIONS A SAMPLER ADMITS INSIDE ONE TEXEL SPAN, measured on this device, because
 * nothing publishes it for Metal and a bound derived from a guess is a guess wearing a derivation
 * (`test/SubTexelPrecision.h`).
 *
 * THE INSTRUMENT IS ONE DISPATCH AND IT NEEDS NO REFERENCE. A two-texel ramp from 0 to 1 is sampled
 * at 65 536 offsets between the two texel centres; a sampler carrying the weight in float would
 * return 65 536 distinct values, and one snapping it to `2^n` divisions returns `2^n`. THE COUNT IS
 * THE ANSWER DIRECTLY, so there is nothing here to compare against and nothing to tune.
 *
 * THE STEP IS THE ANSWER AND THE COUNT IS THE STEP PLUS ONE, which is a fact about the SNAPPING RULE
 * and was measured rather than assumed: 65 536 offsets across the OPEN span return 257 distinct
 * weights whose every step is 1/256. Round-to-nearest is what makes it 257 -- the offsets inside the
 * first half-division round down onto 0 and those inside the last round up onto 1, so `2^n`
 * divisions have `2^n + 1` endpoints. A truncating sampler would have returned 256. Both readings
 * are held below, because a device that made them disagree would be doing something neither
 * describes.
 *
 * THE RAMP IS R32_FLOAT SO THAT THE ANSWER IS THE WEIGHT. An 8-bit ramp would carry the filter's own
 * output quantisation into the same count and the two terms could not be told apart; the picture
 * bound's sampler term is about the WEIGHT (doc/requirements.md I.26.15), so the weight is what this
 * isolates.
 *
 * THE SAMPLER IS THE RENDERER'S OWN -- linear/linear, clamp to edge (`src/render/Renderer.cpp:216`).
 * A probe with a sampler nobody renders through would be measuring a sampler that is not in the
 * path. */
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
#include "SubTexelPrecision.h"

#include "Readback.h"
#include "ShaderPrelude.h"

using outshine::Render::kMslPrelude;
using outshine::Render::Readback;
using outshine::Render::ReadState;
using outshine::Test::kSubTexelDivisions;
using outshine::Test::kSubTexelPrecisionBits;

namespace {

int DeviceErrors = 0;

bool Refused(const void *made, const char *what) {
  if (made) { return false; }
  ++DeviceErrors;
  std::printf("NOTE device refused %s: %s\n", what, SDL_GetError());
  return true;
}

/* Offsets across the span between the two texel centres. Far above any plausible division count, so
 * that "the probe ran out of resolution" and "the sampler carries the weight in float" are
 * distinguishable rather than one indistinct answer. */
constexpr uint32_t kOffsets = 1u << 16;

/* THE DEVICE, AND NOTHING ELSE: no swapchain, no window, no asset. */
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

/* `u` walks the OPEN span between the FIRST TWO texel centres, so every offset the sampler can snap
 * to inside one texel is reachable and neither endpoint is counted twice. At width W those centres
 * are at 0.5/W and 1.5/W, and the shader is handed the width rather than assuming one -- because
 * whether the division count DEPENDS on the width is the question this probe is asked. */
std::string ProbeShader() {
  return std::string(kMslPrelude) + R"(
struct Span { uint offsets; uint texels; };

kernel void probe(uint3 id [[thread_position_in_grid]],
                  constant Span &span [[buffer(0)]],
                  device float *results [[buffer(1)]],
                  texture2d<float> ramp [[texture(0)]],
                  sampler filtered [[sampler(0)]]) {
  if (id.x >= span.offsets) { return; }
  float across = (float(id.x) + 0.5) / float(span.offsets);
  float u = (0.5 + across) / float(span.texels);
  results[id.x] = ramp.sample(filtered, float2(u, 0.5), level(0)).x;
}
)";
}

/* THE RAMP, W TEXELS WIDE, whose first two texels are 0 and 1 -- so the returned value across their
 * span IS the weight and no scaling stands between the measurement and the number. Everything past
 * the second texel is 1 and is never sampled; it is there to make the texture the declared width. */
SDL_GPUTexture *Ramp(SDL_GPUDevice *device, uint32_t width) {
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D;
  wanted.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
  wanted.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = width;
  wanted.height = 1;
  wanted.layer_count_or_depth = 1;
  wanted.num_levels = 1;
  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &wanted);
  if (Refused(texture, "the ramp")) { return nullptr; }

  std::vector<float> texels(width, 1.0f);
  texels[0] = 0.0f;
  const uint32_t bytes = width * (uint32_t)sizeof(float);
  SDL_GPUTransferBufferCreateInfo staging{};
  staging.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  staging.size = bytes;
  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &staging);
  if (Refused(transfer, "the ramp's upload buffer")) { return texture; }
  std::memcpy(SDL_MapGPUTransferBuffer(device, transfer, false), texels.data(), bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = transfer;
  SDL_GPUTextureRegion into{};
  into.texture = texture;
  into.w = width;
  into.h = 1;
  into.d = 1;
  SDL_UploadToGPUTexture(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return texture;
}

SDL_GPUSampler *AsTheRendererSamples(SDL_GPUDevice *device) {
  SDL_GPUSamplerCreateInfo wanted{};
  wanted.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wanted.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wanted.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wanted.min_filter = SDL_GPU_FILTER_LINEAR;
  wanted.mag_filter = SDL_GPU_FILTER_LINEAR;
  wanted.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &wanted);
  Refused(sampler, "the renderer's linear clamp sampler");
  return sampler;
}

std::vector<float> Weights(const Instrument &on, uint32_t width) {
  const uint32_t bytes = kOffsets * (uint32_t)sizeof(float);
  const std::string msl = ProbeShader();

  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(msl.c_str());
  wanted.code_size = msl.size();
  wanted.entrypoint = "probe";
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.num_uniform_buffers = 1;
  wanted.num_samplers = 1;
  wanted.num_readwrite_storage_buffers = 1;
  wanted.threadcount_x = 64;
  wanted.threadcount_y = 1;
  wanted.threadcount_z = 1;
  SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(on.Device, &wanted);
  if (Refused(pipeline, "the probe's compute pipeline")) { return {}; }

  SDL_GPUBufferCreateInfo hold{};
  hold.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
  hold.size = bytes;
  SDL_GPUBuffer *results = SDL_CreateGPUBuffer(on.Device, &hold);
  SDL_GPUTexture *ramp = Ramp(on.Device, width);
  SDL_GPUSampler *sampler = AsTheRendererSamples(on.Device);

  std::vector<float> out;
  if (!Refused(results, "the probe's result buffer") && ramp && sampler) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(on.Device);
    SDL_GPUStorageBufferReadWriteBinding written{};
    written.buffer = results;
    SDL_GPUComputePass *pass = SDL_BeginGPUComputePass(commands, nullptr, 0, &written, 1);
    SDL_BindGPUComputePipeline(pass, pipeline);
    SDL_GPUTextureSamplerBinding bound{};
    bound.texture = ramp;
    bound.sampler = sampler;
    SDL_BindGPUComputeSamplers(pass, 0, &bound, 1);
    const uint32_t span[2] = {kOffsets, width};
    SDL_PushGPUComputeUniformData(commands, 0, span, sizeof span);
    SDL_DispatchGPUCompute(pass, (kOffsets + 63u) / 64u, 1, 1);
    SDL_EndGPUComputePass(pass);
    SDL_SubmitGPUCommandBuffer(commands);

    Readback read;
    if (read.FromBuffer(on.Device, results, bytes) == ReadState::Ready) {
      out.resize(kOffsets);
      std::memcpy(out.data(), read.Rows(), bytes);
    } else {
      ++DeviceErrors;
    }
  }
  SDL_ReleaseGPUSampler(on.Device, sampler);
  SDL_ReleaseGPUTexture(on.Device, ramp);
  SDL_ReleaseGPUBuffer(on.Device, results);
  SDL_ReleaseGPUComputePipeline(on.Device, pipeline);
  return out;
}

/* THE TWO READINGS OF THE SAME RETURNED SET. `Distinct` is `2^n` and `SmallestStep` is `1/2^n`, and
 * the caller holds them against each other. */
struct Divisions {
  size_t Distinct = 0;
  double SmallestStep = 0;
  double LargestStep = 0;
  float Lowest = 0, Highest = 0;
};

Divisions Count(std::vector<float> returned) {
  Divisions found;
  std::sort(returned.begin(), returned.end());
  returned.erase(std::unique(returned.begin(), returned.end()), returned.end());
  found.Distinct = returned.size();
  if (returned.empty()) { return found; }
  found.Lowest = returned.front();
  found.Highest = returned.back();
  for (size_t at = 1; at < returned.size(); ++at) {
    const double step = (double)returned[at] - (double)returned[at - 1];
    if (found.SmallestStep == 0.0 || step < found.SmallestStep) { found.SmallestStep = step; }
    if (step > found.LargestStep) { found.LargestStep = step; }
  }
  return found;
}

/* THE WIDTHS THE PROBE IS RUN AT, AND WHY THERE IS MORE THAN ONE. Two texels is the narrowest span
 * the question has a meaning over. 512 is the width every image in the Khronos corpus actually
 * carries, and it is here because the picture bound applies the term measured at two texels to
 * pictures sampled at 512: if the division count fell with the width -- which is what a fixed-point
 * coordinate of fixed TOTAL width would do -- the term the bound carries would be for a sampler that
 * is not in the path. That was a live hypothesis about `texture-coordinate-test`'s residual and this
 * is the instrument that answers it. */
constexpr uint32_t kWidthsProbed[] = {2, 512};

} // namespace

int main() {
  using namespace outshine::Test;

  const Instrument on;
  CHECK(on.Device != nullptr, "a device answers, so the sampler can be probed at all");
  if (!on.Device) { return Report(); }

  for (uint32_t width : kWidthsProbed) {
    const std::string at = " at " + std::to_string(width) + " texels";
    const std::vector<float> returned = Weights(on, width);
    CHECK(returned.size() == kOffsets,
          ("the probe returns one weight per sub-texel offset" + at).c_str());
    if (returned.size() != kOffsets) { continue; }

    const Divisions found = Count(returned);
    Note(("sub-texel offsets sampled" + at).c_str(), (double)kOffsets, "count");
    Note(("distinct weights returned" + at).c_str(), (double)found.Distinct, "count");
    Note(("smallest positive step between two returned weights" + at).c_str(), found.SmallestStep,
         "of a texel span");
    Note(("largest step between two returned weights" + at).c_str(), found.LargestStep,
         "of a texel span");
    Note(("lowest weight returned" + at).c_str(), found.Lowest, "of a texel span");
    Note(("highest weight returned" + at).c_str(), found.Highest, "of a texel span");

    /* A COUNT AT THE PROBE'S OWN RESOLUTION IS NOT A MEASUREMENT OF THE DEVICE. Refused rather than
     * reported, because "65 536" would read as a division count and would be this file's number. */
    CHECK(found.Distinct * 4 < kOffsets,
          ("the probe resolves the division: the distinct count is far below the offsets sampled, "
           "so the snapping and not this dispatch is what set it" + at).c_str());

    /* THE DIVISION ITSELF, at nine digits: both readings come off f32 values of order 1, so their
     * own disagreement floor is around 1e-7. */
    CHECK_NEAR(found.SmallestStep, 1.0 / (double)kSubTexelDivisions, 1.0e-7, "of a texel span",
               ("one division of the texel span is the declared 2^-n (test/SubTexelPrecision.h)" +
                at)
                   .c_str());
    CHECK_NEAR(found.LargestStep, 1.0 / (double)kSubTexelDivisions, 1.0e-7, "of a texel span",
               ("the divisions are uniform: the largest step is the smallest one, so the snapping "
                "is a division of the span and not a curve over it" + at)
                   .c_str());
    CHECK(found.Distinct == (size_t)kSubTexelDivisions + 1u,
          ("the open span returns one weight per division endpoint, which is the same count reached "
           "by different arithmetic and is what says the snapping rounds to nearest rather than "
           "truncates" + at)
              .c_str());
    Note(("sub-texel precision bits this device carries" + at).c_str(),
         std::log2((double)found.Distinct - 1.0), "bits");
  }
  Note("the declared bit count", (double)kSubTexelPrecisionBits, "bits");

  CHECK(DeviceErrors == 0, "the device reported no error over the probe");
  Covers("I.26.15 the sampler term of the picture bound: how finely this device divides one texel "
         "span is measured here rather than looked up, and the picture bound derives 255 * "
         "2^-(n+1) from it");
  return Report();
}
