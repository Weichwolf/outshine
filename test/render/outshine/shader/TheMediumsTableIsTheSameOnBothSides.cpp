#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "MediumTransmittanceStage.h"
#include "ParticipatingMedium.h"
#include "Readback.h"

using outshine::Render::Gpu;
using outshine::Render::kTransmittanceLutHeight;
using outshine::Render::kTransmittanceLutWidth;
using outshine::Render::kTransmittanceSteps;
using outshine::Render::Medium;
using outshine::Render::MediumTransmittance;
using outshine::Render::MediumTransmittanceParams;
using outshine::Render::MediumTransmittanceStage;
using outshine::Render::PassRecording;
using outshine::Render::Readback;
using outshine::Render::ReadState;

namespace {

float FromHalf(uint16_t bits) {
  const uint32_t sign = (uint32_t)(bits >> 15) << 31;
  const uint32_t exponent = (bits >> 10) & 0x1Fu;
  const uint32_t mantissa = bits & 0x3FFu;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      int shift = 0;
      uint32_t left = mantissa;
      while ((left & 0x400u) == 0) {
        left <<= 1;
        ++shift;
      }
      assembled = sign | ((uint32_t)(113 - shift) << 23) | ((left & 0x3FFu) << 13);
    } else {
      assembled = sign;
    }
  } else if (exponent == 31) {
    assembled = sign | 0x7F800000u | (mantissa << 13);
  } else {
    assembled = sign | ((exponent - 15u + 127u) << 23) | (mantissa << 13);
  }
  float out;
  std::memcpy(&out, &assembled, sizeof out);
  return out;
}

constexpr double kHalfSubnormalStep = 5.9604644775390625e-8;

constexpr double kHalfSmallestNormal = 6.103515625e-5;

double HalfStep(double value) {
  if (std::fabs(value) < kHalfSmallestNormal) { return kHalfSubnormalStep; }
  const int exponent = (int)std::floor(std::log2(std::fabs(value)));
  return std::ldexp(1.0, exponent - 10);
}

class Instrument {
public:
  Instrument() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { return; }
    Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!Device) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  }
  ~Instrument() {
    if (Device == nullptr) { return; }
    SDL_DestroyGPUDevice(Device);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
  Instrument(const Instrument &) = delete;
  Instrument &operator=(const Instrument &) = delete;

  SDL_GPUDevice *Device = nullptr;
};

const char *const kChannel[3] = {"red", "green", "blue"};

constexpr double kAllowedSteps = 4.0;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Instrument on;
  CHECK(on.Device != nullptr, "a device the medium's kernel can run on");
  if (on.Device == nullptr) {
    std::printf("NOTE the device refused: %s\n", SDL_GetError());
    return Report();
  }

  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D;
  wanted.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  wanted.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = kTransmittanceLutWidth;
  wanted.height = kTransmittanceLutHeight;
  wanted.layer_count_or_depth = 1;
  wanted.num_levels = 1;
  wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture *const lut = SDL_CreateGPUTexture(on.Device, &wanted);
  CHECK(lut != nullptr, "and a table for it to write");
  if (lut == nullptr) {
    std::printf("NOTE the device refused the table: %s\n", SDL_GetError());
    return Report();
  }

  Gpu handles{};
  handles.Device = on.Device;

  MediumTransmittanceStage stage;
  std::string why;
  const bool built = stage.Configure(handles, lut, why);
  if (!built) { std::printf("REFUSED %s\n", why.c_str()); }
  CHECK(built, "**THE STAGE THE RENDERER SHIPS IS THE ONE UNDER TEST, not a copy of its kernel.** "
               "A twin test that pasted the shader would go green over a stage nobody could run");
  if (!built) { return Report(); }

  const Medium medium;
  stage.Declare(medium);
  CHECK(!stage.Settled(), "a medium just declared has not reached the table yet");

  {
    SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(on.Device);
    SDL_GPUStorageTextureReadWriteBinding written{};
    written.texture = lut;
    PassRecording into{commands, nullptr,
                       SDL_BeginGPUComputePass(commands, &written, 1, nullptr, 0)};
    stage.Encode(into);
    SDL_EndGPUComputePass(into.Dispatch);
    SDL_SubmitGPUCommandBuffer(commands);
  }
  CHECK(stage.Settled(),
        "**AND ONCE IT IS THERE THE STAGE SAYS SO, which is what keeps this off the frame path.** "
        "The table is a function of the medium alone; a medium that has not changed has no work "
        "in it, and a stage that redispatched every frame would spend 655 360 ray-march steps "
        "producing a texture identical to the one already bound");

  std::vector<uint8_t> raw;
  {
    Readback read;
    const ReadState state = read.FromTexture(on.Device, lut, kTransmittanceLutWidth,
                                             kTransmittanceLutHeight, 8u);
    CHECK(state == ReadState::Ready, "the table reads back");
    if (state != ReadState::Ready) { return Report(); }
    raw.assign(read.Rows(),
               read.Rows() + (size_t)kTransmittanceLutWidth * kTransmittanceLutHeight * 8u);
  }

  size_t walked = 0;
  double worstSteps = 0.0;
  uint32_t worstX = 0, worstY = 0;
  int worstChannel = 0;
  double worstHere = 0.0, worstThere = 0.0;
  size_t past = 0;
  double alphaLow = 2.0;
  size_t pastByRow[kTransmittanceLutHeight] = {};
  size_t underflowed = 0;
  size_t subnormal = 0;
  double reddestUnderflow = 0.0;
  for (uint32_t y = 0; y < kTransmittanceLutHeight; ++y) {
    for (uint32_t x = 0; x < kTransmittanceLutWidth; ++x) {
      const uint16_t *const texel =
          reinterpret_cast<const uint16_t *>(raw.data()) + ((size_t)y * kTransmittanceLutWidth + x) * 4u;
      float radiusKm = 0.0f, cosZenith = 0.0f;
      MediumTransmittanceParams(medium, ((float)x + 0.5f) / (float)kTransmittanceLutWidth,
                                ((float)y + 0.5f) / (float)kTransmittanceLutHeight, &radiusKm,
                                &cosZenith);
      float here[3];
      MediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, here);
      alphaLow = std::fmin(alphaLow, (double)FromHalf(texel[3]));
      for (int channel = 0; channel < 3; ++channel) {
        const double there = (double)FromHalf(texel[channel]);
        if ((double)here[channel] < kHalfSmallestNormal) { ++subnormal; }
        if (there == 0.0 && here[channel] > 0.0f) {
          ++underflowed;
          reddestUnderflow = std::fmax(reddestUnderflow, (double)here[channel]);
        }
        const double steps = std::fabs(there - (double)here[channel]) / HalfStep((double)here[channel]);
        if (steps > kAllowedSteps) {
          ++past;
          ++pastByRow[y];
        }
        if (steps > worstSteps) {
          worstSteps = steps;
          worstX = x;
          worstY = y;
          worstChannel = channel;
          worstHere = (double)here[channel];
          worstThere = there;
        }
      }
      ++walked;
    }
  }

  Note("texels compared", (double)walked, "texels");
  Note("channels compared", (double)walked * 3.0, "values");
  std::printf("NOTE the widest disagreement sits at x=%u y=%u in %s: the twin says %.9e, the device "
              "%.9e -- %.2f half-steps, %.4f %% of the value\n",
              worstX, worstY, kChannel[worstChannel], worstHere, worstThere, worstSteps,
              worstHere > 0.0 ? 100.0 * std::fabs(worstThere - worstHere) / worstHere : 0.0);
  Note("values past the bound", (double)past, "values");
  Note("values the half format can only hold subnormally", (double)subnormal, "values");
  Note("values that underflowed to zero", (double)underflowed, "values");
  Note("the largest of those", reddestUnderflow, "transmittance");
  for (uint32_t y = 0; y < kTransmittanceLutHeight; ++y) {
    if (pastByRow[y] == 0) { continue; }
    float radiusKm = 0.0f, cosZenith = 0.0f;
    MediumTransmittanceParams(medium, 0.5f, ((float)y + 0.5f) / (float)kTransmittanceLutHeight,
                              &radiusKm, &cosZenith);
    std::printf("NOTE   row %u -- %.3f km up -- carries %zu of them\n", y,
                (double)radiusKm - (double)medium.BottomRadiusKm, pastByRow[y]);
  }

  CHECK(walked == (size_t)kTransmittanceLutWidth * kTransmittanceLutHeight,
        "**THE POPULATION IS EVERY TEXEL OF THE TABLE, all 16 384 of them, and not a sample.** A "
        "shader twin checked over a handful of points is a claim about those points");
  CHECK(past == 0,
        "**AND THE DEVICE AGREES WITH THE C++ TWIN EVERYWHERE, to within the storage.** The bound "
        "is FOUR HALF-STEPS OF THE VALUE ITSELF rather than a fixed decimal: a 16-bit float "
        "carries ten mantissa bits, so its step at 0.9 is 4.9e-4 and at 4e-5 is 2.4e-8, and one "
        "absolute tolerance across that range would be vacuous at the top and impossible at the "
        "bottom. Four steps is the rounding of the write plus what Metal's fast exp and sqrt "
        "differ from libm's over forty accumulated steps");
  CHECK(alphaLow == 1.0,
        "and every texel's fourth channel is one, which is how a table says it was written rather "
        "than merely allocated -- the check that a kernel which never ran would fail");

  {
    float zenith[3];
    const uint16_t *const top =
        reinterpret_cast<const uint16_t *>(raw.data()) + (size_t)0u * 4u;
    MediumTransmittanceParams(medium, 0.5f / (float)kTransmittanceLutWidth,
                              0.5f / (float)kTransmittanceLutHeight, zenith, zenith + 1);
    (void)zenith;
    for (int channel = 0; channel < 3; ++channel) {
      Note(kChannel[channel], (double)FromHalf(top[channel]), "at the table's first texel");
    }
    CHECK(FromHalf(top[0]) > FromHalf(top[1]) && FromHalf(top[1]) > FromHalf(top[2]),
          "and the picture the device produced reddens with height exactly as the closed form "
          "does -- read off the device's own bytes rather than recomputed here");
  }

  SDL_ReleaseGPUTexture(on.Device, lut);
  Covers("I.18.2 the medium's transmittance table is produced by a compute stage on the device, "
         "and every texel of it agrees with the engine's own C++ twin");
  return Report();
}
