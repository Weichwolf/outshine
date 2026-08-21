#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "MediumMultiScatterStage.h"
#include "MediumTransmittanceStage.h"
#include "ParticipatingMedium.h"
#include "Readback.h"

using outshine::Render::Gpu;
using outshine::Render::kMultiScatterLutSize;
using outshine::Render::kTransmittanceLutHeight;
using outshine::Render::kTransmittanceLutWidth;
using outshine::Render::kTransmittanceSteps;
using outshine::Render::Medium;
using outshine::Render::MediumMultiScatterStage;
using outshine::Render::MediumMultiScatterTexel;
using outshine::Render::MediumTransmittance;
using outshine::Render::MediumTransmittanceStage;
using outshine::Render::MediumTransmittanceUv;
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

struct HalfTable {
  std::vector<float> Texels;

  void Sample(const Medium &medium, float radiusKm, float cosZenith, float out[3]) const {
    float u = 0.0f, v = 0.0f;
    MediumTransmittanceUv(medium, radiusKm, cosZenith, &u, &v);
    const float x = std::fmin(std::fmax(u * (float)kTransmittanceLutWidth - 0.5f, 0.0f),
                              (float)kTransmittanceLutWidth - 1.0f);
    const float y = std::fmin(std::fmax(v * (float)kTransmittanceLutHeight - 0.5f, 0.0f),
                              (float)kTransmittanceLutHeight - 1.0f);
    const size_t x0 = (size_t)x, y0 = (size_t)y;
    const size_t x1 = x0 + 1 < kTransmittanceLutWidth ? x0 + 1 : x0;
    const size_t y1 = y0 + 1 < kTransmittanceLutHeight ? y0 + 1 : y0;
    const float fx = x - (float)x0, fy = y - (float)y0;
    for (int channel = 0; channel < 3; ++channel) {
      const auto at = [&](size_t tx, size_t ty) {
        return Texels[(ty * kTransmittanceLutWidth + tx) * 3u + (size_t)channel];
      };
      out[channel] = (1.0f - fy) * ((1.0f - fx) * at(x0, y0) + fx * at(x1, y0)) +
                     fy * ((1.0f - fx) * at(x0, y1) + fx * at(x1, y1));
    }
  }
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Instrument on;
  CHECK(on.Device != nullptr, "a device the chain can run on");
  if (on.Device == nullptr) {
    std::printf("NOTE the device refused: %s\n", SDL_GetError());
    return Report();
  }

  const auto storageTexture = [&](uint32_t width, uint32_t height) {
    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    wanted.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    wanted.width = width;
    wanted.height = height;
    wanted.layer_count_or_depth = 1;
    wanted.num_levels = 1;
    wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(on.Device, &wanted);
  };
  SDL_GPUTexture *const transmittance =
      storageTexture(kTransmittanceLutWidth, kTransmittanceLutHeight);
  SDL_GPUTexture *const multiScatter = storageTexture(kMultiScatterLutSize, kMultiScatterLutSize);

  SDL_GPUSamplerCreateInfo sampler{};
  sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  SDL_GPUSampler *const lut = SDL_CreateGPUSampler(on.Device, &sampler);
  CHECK(transmittance != nullptr && multiScatter != nullptr && lut != nullptr,
        "two tables and a sampler");
  if (transmittance == nullptr || multiScatter == nullptr || lut == nullptr) { return Report(); }

  Gpu handles{};
  handles.Device = on.Device;

  MediumTransmittanceStage first;
  MediumMultiScatterStage second;
  std::string why;
  const bool builtFirst = first.Configure(handles, transmittance, why);
  if (!builtFirst) { std::printf("REFUSED %s\n", why.c_str()); }
  const bool builtSecond = second.Configure(handles, transmittance, lut, multiScatter, why);
  if (!builtSecond) { std::printf("REFUSED %s\n", why.c_str()); }
  CHECK(builtFirst && builtSecond,
        "**BOTH SHIPPING STAGES BUILD, AND THE SECOND READS WHAT THE FIRST WROTE** -- this test "
        "runs the chain the plan would run, in two passes of one command buffer, which is exactly "
        "the read-after-write the plan compiler now refuses to merge");
  if (!builtFirst || !builtSecond) { return Report(); }

  const Medium medium;
  first.Declare(medium);
  second.Declare(medium);
  {
    SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(on.Device);
    {
      SDL_GPUStorageTextureReadWriteBinding written{};
      written.texture = transmittance;
      PassRecording into{commands, nullptr,
                         SDL_BeginGPUComputePass(commands, &written, 1, nullptr, 0)};
      first.Encode(into);
      SDL_EndGPUComputePass(into.Dispatch);
    }
    {
      SDL_GPUStorageTextureReadWriteBinding written{};
      written.texture = multiScatter;
      PassRecording into{commands, nullptr,
                         SDL_BeginGPUComputePass(commands, &written, 1, nullptr, 0)};
      second.Encode(into);
      SDL_EndGPUComputePass(into.Dispatch);
    }
    SDL_SubmitGPUCommandBuffer(commands);
  }

  HalfTable table;
  {
    Readback read;
    const ReadState state = read.FromTexture(on.Device, transmittance, kTransmittanceLutWidth,
                                             kTransmittanceLutHeight, 8u);
    CHECK(state == ReadState::Ready, "the transmittance table reads back");
    if (state != ReadState::Ready) { return Report(); }
    const uint16_t *const half = reinterpret_cast<const uint16_t *>(read.Rows());
    table.Texels.resize((size_t)kTransmittanceLutWidth * kTransmittanceLutHeight * 3u);
    for (size_t texel = 0; texel < (size_t)kTransmittanceLutWidth * kTransmittanceLutHeight;
         ++texel) {
      for (int channel = 0; channel < 3; ++channel) {
        table.Texels[texel * 3u + (size_t)channel] = FromHalf(half[texel * 4u + (size_t)channel]);
      }
    }
  }

  std::vector<uint8_t> raw;
  {
    Readback read;
    const ReadState state =
        read.FromTexture(on.Device, multiScatter, kMultiScatterLutSize, kMultiScatterLutSize, 8u);
    CHECK(state == ReadState::Ready, "and the multiple scattering table reads back");
    if (state != ReadState::Ready) { return Report(); }
    raw.assign(read.Rows(),
               read.Rows() + (size_t)kMultiScatterLutSize * kMultiScatterLutSize * 8u);
  }

  const auto toSun = [&](float radiusKm, float cosZenith, float out[3]) {
    table.Sample(medium, radiusKm, cosZenith, out);
  };

  size_t walked = 0;
  size_t past = 0;
  double worstApart = 0.0;
  double worstScale = 1.0;
  uint32_t worstX = 0, worstY = 0;
  double high = 0.0;
  for (uint32_t y = 0; y < kMultiScatterLutSize; ++y) {
    for (uint32_t x = 0; x < kMultiScatterLutSize; ++x) {
      const uint16_t *const texel = reinterpret_cast<const uint16_t *>(raw.data()) +
                                    ((size_t)y * kMultiScatterLutSize + x) * 4u;
      float luminance[3], transfer[3];
      MediumMultiScatterTexel(medium, ((float)x + 0.5f) / (float)kMultiScatterLutSize,
                              ((float)y + 0.5f) / (float)kMultiScatterLutSize, toSun, luminance,
                              transfer);
      for (int channel = 0; channel < 3; ++channel) {
        const double here = (double)luminance[channel] / (1.0 - (double)transfer[channel]);
        const double there = (double)FromHalf(texel[channel]);
        high = std::fmax(high, here);
        const double scale = std::fmax(here, 2.0e-3);
        const double apart = std::fabs(there - here);
        if (apart / scale > 0.01) { ++past; }
        if (apart / scale > worstApart / worstScale) {
          worstApart = apart;
          worstScale = scale;
          worstX = x;
          worstY = y;
        }
      }
      ++walked;
    }
  }

  Note("texels compared", (double)walked, "texels");
  Note("the table's largest value", high, "per unit sun illuminance");
  std::printf("NOTE the widest disagreement sits at x=%u y=%u: %.6e apart on a scale of %.6e -- "
              "%.4f %%\n",
              worstX, worstY, worstApart, worstScale, 100.0 * worstApart / worstScale);
  Note("values past one percent", (double)past, "of 3072");

  CHECK(walked == (size_t)kMultiScatterLutSize * kMultiScatterLutSize,
        "every texel of the 32x32 table is compared");
  CHECK(past == 0,
        "**THE DEVICE'S SECOND-ORDER TABLE AGREES WITH THE TWIN TO ONE PERCENT, ACROSS THE "
        "CHAIN.** The twin samples the DEVICE'S OWN half-precision transmittance table bilinearly "
        "-- not its float sibling -- so the comparison isolates the multiple scattering kernel from "
        "the first stage's storage. One percent, floored at 2e-3 absolute, covers the half write "
        "of the result plus 1280 accumulations of sampler-vs-C++ bilinear differences; the floor "
        "keeps the night texels, where the value is 1e-6 of noon's, from demanding relative "
        "agreement below the format's own resolution");

  SDL_ReleaseGPUSampler(on.Device, lut);
  SDL_ReleaseGPUTexture(on.Device, transmittance);
  SDL_ReleaseGPUTexture(on.Device, multiScatter);
  Covers("I.18.4 the multiple scattering table is computed on the device from the device's own "
         "transmittance table, and the chain agrees with the C++ twin texel for texel");
  return Report();
}
