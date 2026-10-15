#include "AerialPerspectiveStage.h"
#include "Check.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "StageSubmission.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSkyWidth = 16;
constexpr uint32_t kSkyHeight = 8;
constexpr uint32_t kVeilWidth = 64;
constexpr uint32_t kVeilHeight = 32;
constexpr auto kColour = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
constexpr double kSamplerTolerance = 2.0 / (kSkyWidth - 1u) / 256.0;

OwnedTexture Texture(SDL_GPUDevice *device,
                     uint32_t width,
                     uint32_t height,
                     SDL_GPUTextureFormat format = kColour) {
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = format;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  info.width = width;
  info.height = height;
  info.layer_count_or_depth = info.num_levels = 1;
  return {device, SDL_CreateGPUTexture(device, &info)};
}

bool Upload(SDL_GPUDevice *device,
            SDL_GPUTexture *texture,
            uint32_t width,
            uint32_t height,
            std::span<const float> pixels) {
  SDL_GPUTransferBufferCreateInfo info{};
  info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  info.size = static_cast<uint32_t>(pixels.size_bytes());
  const OwnedTransfer transfer(device, SDL_CreateGPUTransferBuffer(device, &info));
  if (!transfer) { return false; }
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer.Get(), false);
  if (mapped == nullptr) { return false; }
  std::memcpy(mapped, pixels.data(), pixels.size_bytes());
  SDL_UnmapGPUTransferBuffer(device, transfer.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (commands == nullptr) { return false; }
  auto *pass = SDL_BeginGPUCopyPass(commands);
  if (pass == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = transfer.Get();
  SDL_GPUTextureRegion target{};
  target.texture = texture;
  target.w = width;
  target.h = height;
  target.d = 1;
  SDL_UploadToGPUTexture(pass, &source, &target, false);
  SDL_EndGPUCopyPass(pass);
  return SDL_SubmitGPUCommandBuffer(commands);
}

struct Sources {
  OwnedTexture Scene, Depth, Sky, Veil, Output;
  OwnedSampler Exact, Linear;
  Medium Air = kEarthAir;

  explicit Sources(SDL_GPUDevice *device)
      : Scene(Texture(device, 1, 1)),
        Depth(Texture(device, 1, 1, SDL_GPU_TEXTUREFORMAT_R32_FLOAT)),
        Sky(Texture(device, kSkyWidth, kSkyHeight)),
        Veil(Texture(device, kVeilWidth, kVeilHeight)),
        Output(Texture(device, 1, 1)) {
    Air.BottomRadiusKm = 1;
    Air.TopRadiusKm = 2;
    SDL_GPUSamplerCreateInfo info{};
    info.address_mode_u = info.address_mode_v = info.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    Exact = {device, SDL_CreateGPUSampler(device, &info)};
    info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
    Linear = {device, SDL_CreateGPUSampler(device, &info)};
  }

  bool Prepare(SDL_GPUDevice *device) const {
    if (!Scene || !Depth || !Sky || !Veil || !Output || !Exact || !Linear) { return false; }
    std::vector<float> sky(kSkyWidth * kSkyHeight * 4u);
    for (uint32_t y = 0; y < kSkyHeight; ++y) {
      for (uint32_t x = 0; x < kSkyWidth; ++x) {
        const size_t at = (y * kSkyWidth + x) * 4u;
        sky[at] = y < kSkyHeight / 2u ? .25f + 2.0f * x / (kSkyWidth - 1u) : 1000.0f;
        sky[at + 1] = y < kSkyHeight / 2u ? .5f : 0.0f;
        sky[at + 2] = y < kSkyHeight / 2u ? 1.0f : 0.0f;
        sky[at + 3] = 1;
      }
    }
    std::vector<float> veil(kVeilWidth * kVeilHeight * 4u, 1);
    for (uint32_t y = 0; y < kVeilHeight; ++y) {
      for (uint32_t x = 0; x < kVeilWidth; ++x) {
        const auto look = medium_core::mediumTransmittanceParams(
            Air, {(x + .5f) / kVeilWidth, (y + .5f) / kVeilHeight});
        const float through = std::exp(-mediumTopReach(Air, look.RadiusKm, look.CosZenith));
        const size_t at = (y * kVeilWidth + x) * 4u;
        for (size_t channel = 0; channel < 3; ++channel) { veil[at + channel] = through; }
      }
    }
    return Upload(device, Scene.Get(), 1, 1, std::array<float, 4>{0, 0, 0, 1}) &&
           Upload(device, Depth.Get(), 1, 1, std::array<float, 1>{1}) &&
           Upload(device, Sky.Get(), kSkyWidth, kSkyHeight, sky) &&
           Upload(device, Veil.Get(), kVeilWidth, kVeilHeight, veil);
  }
};

std::array<float, 4> Draw(SDL_GPUDevice *device,
                          const Sources &source,
                          AerialPerspectiveStage &stage,
                          Vec3f up,
                          Vec3f look,
                          Vec3f sun) {
  stage.Declare(source.Air,
                {.SunDir = sun,
                 .WorldUp = up,
                 .IlluminanceLux = 1,
                 .EyeHeightM = (0.5f - kMediumGroundLiftKm) * 1000});
  stage.SetBasis({.Right = {}, .Up = {}, .Forward = look});
  stage.SetDepthReconstruction({{250, 0, 1, 0}});
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  CHECK(commands != nullptr, "aerial commands allocate");
  if (commands == nullptr) { return {}; }
  SDL_GPUColorTargetInfo target{};
  target.texture = source.Output.Get();
  target.load_op = SDL_GPU_LOADOP_CLEAR;
  target.store_op = SDL_GPU_STOREOP_STORE;
  auto *pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
  CHECK(pass != nullptr, "the real aerial perspective pass opens");
  if (pass == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return {};
  }
  StageSubmission submission;
  stage.Encode({}, {.Commands = commands, .Pass = pass, .Submission = submission});
  SDL_EndGPURenderPass(pass);
  CHECK(SDL_SubmitGPUCommandBuffer(commands), "aerial shading submits");
  Readback read;
  CHECK(read.FromTexture(device, source.Output.Get(), {.WidthPx = 1, .HeightPx = 1}, 16) ==
            ReadState::Ready,
        "the actual fragment output reaches readback");
  std::array<float, 4> pixel{};
  if (read.Rows() != nullptr) { std::memcpy(pixel.data(), read.Rows(), sizeof(pixel)); }
  return pixel;
}

void CheckDirections(SDL_GPUDevice *device) {
  const Sources source(device);
  const bool prepared = source.Prepare(device);
  CHECK(prepared, "independent sky/ground colours and extinction upload");
  if (!prepared) { return; }
  AerialPerspectiveStage stage;
  std::string error;
  Gpu gpu{};
  gpu.Device = device;
  const bool configured = stage.Configure(gpu,
                                          {.Scene = source.Scene.Get(),
                                           .Depth = source.Depth.Get(),
                                           .SkyView = source.Sky.Get(),
                                           .Transmittance = source.Veil.Get(),
                                           .Exact = source.Exact.Get(),
                                           .Lut = source.Linear.Get()},
                                          kColour,
                                          error);
  CHECK(configured, error.empty() ? "the production aerial shader configures" : error.c_str());
  if (!configured) { return; }
  for (const Vec3f up : {Vec3f{{0, 1, 0}}, Vec3f{{1, 0, 0}}}) {
    const Vec3f along = up[0] == 0 ? Vec3f{{1, 0, 0}} : Vec3f{{0, 1, 0}};
    const Vec3f across{{0, 0, 1}};
    const float diagonal = std::sqrt(0.5f);
    const std::array<Vec3f, 6> suns{
        up, up * -1.0f, along, along * -1.0f, across, (along + across) * diagonal};
    const std::array<float, 6> sunColumns{
        0, 0, 0, 1, diagonal, std::sqrt((1.0f - diagonal) * 0.5f)};
    for (size_t index = 0; index < suns.size(); ++index) {
      for (const Vec3f look : {up, up * -1.0f, along}) {
        const auto pixel = Draw(device, source, stage, up, look, suns[index]);
        for (const float value : pixel) {
          CHECK(std::isfinite(value), "zenith and nadir sun/look directions keep finite radiance");
        }
        CHECK(pixel[2] > .01f && pixel[2] < 1, "a finite segment receives positive sky scattering");
        if (!(pixel[2] > .01f)) { continue; }
        const float column = Dot(look, along) > .5f ? sunColumns[index] : 0;
        CHECK_NEAR(pixel[0] / pixel[2],
                   .25 + 2 * column,
                   kSamplerTolerance,
                   "radiance ratio",
                   "relative sun azimuth uses the correct finite texel centres");
        CHECK_NEAR(pixel[1] / pixel[2],
                   .5,
                   1e-4,
                   "radiance ratio",
                   "horizon and downward rays exclude the bright virtual ground");
      }
    }
  }
}
}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"), "assertions fail without a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  {
    OwnedDevice device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                               SDL_GPU_SHADERFORMAT_METALLIB,
                                           false,
                                           nullptr));
    CHECK(device, "an actual GPU backend is available");
    if (device) { CheckDirections(device.Get()); }
  }
  SDL_Quit();
  return Report();
}
