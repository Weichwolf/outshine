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
constexpr uint32_t kSourceWidth = 16;
constexpr uint32_t kSourceHeight = 8;
constexpr uint32_t kVeilWidth = 64;
constexpr uint32_t kVeilHeight = 32;
constexpr auto kColour = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
constexpr std::array<float, 4> kScene = {.1f, .2f, .3f, .7f};
constexpr std::array<float, 3> kSolar = {.8f, .6f, .3f};
constexpr std::array<float, 3> kPsi = {.01f, .02f, .03f};

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
  OwnedTexture Scene, Depth, Multi, Veil, Output;
  OwnedSampler Exact, Linear;
  Medium Air = kEarthAir;

  explicit Sources(SDL_GPUDevice *device)
      : Scene(Texture(device, 1, 1)),
        Depth(Texture(device, 1, 1, SDL_GPU_TEXTUREFORMAT_R32_FLOAT)),
        Multi(Texture(device, kSourceWidth, kSourceHeight)),
        Veil(Texture(device, kVeilWidth, kVeilHeight)),
        Output(Texture(device, 1, 1)) {
    Air.BottomRadiusKm = 1;
    Air.TopRadiusKm = 2;
    Air.RayleighScaleHeightKm = Air.MieScaleHeightKm = 1e20f;
    Air.RayleighScatteringPerKm = {.1f, .2f, .3f};
    Air.MieScatteringPerKm = .03f;
    Air.MieExtinctionPerKm = .04f;
    Air.OzoneAbsorptionPerKm = {};
    SDL_GPUSamplerCreateInfo info{};
    info.address_mode_u = info.address_mode_v = info.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    Exact = {device, SDL_CreateGPUSampler(device, &info)};
    info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
    Linear = {device, SDL_CreateGPUSampler(device, &info)};
  }

  bool Prepare(SDL_GPUDevice *device) const {
    if (!Scene || !Depth || !Multi || !Veil || !Output || !Exact || !Linear) { return false; }
    std::vector<float> multi(kSourceWidth * kSourceHeight * 4u, 1);
    for (size_t pixel = 0; pixel < multi.size() / 4u; ++pixel) {
      for (size_t channel = 0; channel < 3; ++channel) {
        multi[pixel * 4u + channel] = kPsi[channel];
      }
    }
    std::vector<float> veil(kVeilWidth * kVeilHeight * 4u, 1);
    for (size_t pixel = 0; pixel < veil.size() / 4u; ++pixel) {
      for (size_t channel = 0; channel < 3; ++channel) {
        veil[pixel * 4u + channel] = kSolar[channel];
      }
    }
    return Upload(device, Scene.Get(), 1, 1, kScene) &&
           Upload(device, Depth.Get(), 1, 1, std::array<float, 1>{1}) &&
           Upload(device, Multi.Get(), kSourceWidth, kSourceHeight, multi) &&
           Upload(device, Veil.Get(), kVeilWidth, kVeilHeight, veil);
  }
};

std::array<float, 4> Draw(SDL_GPUDevice *device,
                          const Sources &source,
                          AerialPerspectiveStage &stage,
                          Vec3f up,
                          Vec3f look,
                          Vec3f sun) {
  stage.Declare(source.Air, {.SunDir = sun, .WorldUp = up, .IlluminanceLux = 1, .EyeHeightM = 500});
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
  CHECK(prepared, "independent homogeneous source and scene products upload");
  if (!prepared) { return; }
  AerialPerspectiveStage stage;
  std::string error;
  Gpu gpu{};
  gpu.Device = device;
  const bool configured = stage.Configure(gpu,
                                          {.Scene = source.Scene.Get(),
                                           .Depth = source.Depth.Get(),
                                           .MultiScatter = source.Multi.Get(),
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
    for (const auto &sun : suns) {
      for (const Vec3f look : {up, up * -1.0f, along}) {
        const auto pixel = Draw(device, source, stage, up, look, sun);
        const double cosine = Dot(look, sun);
        const double g = source.Air.MiePhaseG;
        const double pi = std::acos(-1.0);
        const double phaseR = 3.0 * (1.0 + cosine * cosine) / (16.0 * pi);
        const double phaseM = 3.0 / (8.0 * pi) * (1.0 - g * g) / (2.0 + g * g) *
                              (1.0 + cosine * cosine) /
                              std::pow(1.0 + g * g - 2.0 * g * cosine, 1.5);
        const bool lit = Dot(sun, up) >= 0;
        for (size_t channel = 0; channel < 3; ++channel) {
          const double rayleigh = source.Air.RayleighScatteringPerKm[channel];
          const double mie = source.Air.MieScatteringPerKm;
          const double extinction = rayleigh + source.Air.MieExtinctionPerKm;
          const double emission = (lit ? kSolar[channel] : 0) * (rayleigh * phaseR + mie * phaseM) +
                                  (rayleigh + mie) * kPsi[channel];
          const double through = std::exp(-extinction * .25);
          const double expected =
              kScene[channel] * through + emission * -std::expm1(-extinction * .25) / extinction;
          CHECK_NEAR(pixel[channel],
                     expected,
                     std::max(1e-7, std::fabs(expected) * 1e-5),
                     "radiance",
                     "the real aerial pass composes finite RGB-T and source exactly once");
        }
        CHECK(pixel[3] == kScene[3], "finite aerial transport preserves scene alpha");
      }
    }
  }
  CHECK(Upload(device, source.Depth.Get(), 1, 1, std::array<float, 1>{0}),
        "background depth uploads");
  const auto background = Draw(device, source, stage, {{0, 1, 0}}, {{1, 0, 0}}, {{0, 1, 0}});
  CHECK(background == kScene, "background bypass is bit exact and does not shade sky twice");
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
