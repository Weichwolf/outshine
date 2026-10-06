#include "Check.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "StageSubmission.h"
#include "SurfaceReprojectionStage.h"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr int kSide = 17;
constexpr size_t kCentre = kSide * (kSide / 2) + kSide / 2;

bool Land(SDL_GPUDevice *device, SDL_GPUCommandBuffer *commands) {
  OwnedFence fence(device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
  auto *handle = fence.Get();
  return fence && SDL_WaitForGPUFences(device, true, &handle, 1);
}

OwnedTexture
Image(SDL_GPUDevice *device, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage) {
  SDL_GPUTextureCreateInfo image{};
  image.type = SDL_GPU_TEXTURETYPE_2D;
  image.format = format;
  image.usage = usage;
  image.width = image.height = kSide;
  image.layer_count_or_depth = image.num_levels = 1;
  return {device, SDL_CreateGPUTexture(device, &image)};
}

OwnedTexture Upload(SDL_GPUDevice *device, const std::array<float, 4> &value, bool hole) {
  auto image =
      Image(device, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, SDL_GPU_TEXTUREUSAGE_SAMPLER);
  std::vector<float> samples(kSide * kSide * 4);
  for (size_t at = 0; at < samples.size() / 4; ++at) {
    std::copy(value.begin(), value.end(), samples.begin() + static_cast<ptrdiff_t>(at * 4));
  }
  if (hole) { samples[0] = 0; }
  SDL_GPUTransferBufferCreateInfo allocation{};
  allocation.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  allocation.size = static_cast<uint32_t>(samples.size() * sizeof(float));
  OwnedTransfer staging(device, SDL_CreateGPUTransferBuffer(device, &allocation));
  if (!image || !staging) { return {}; }
  void *mapped = SDL_MapGPUTransferBuffer(device, staging.Get(), false);
  if (mapped == nullptr) { return {}; }
  std::memcpy(mapped, samples.data(), allocation.size);
  SDL_UnmapGPUTransferBuffer(device, staging.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  auto *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureTransferInfo from{};
  from.transfer_buffer = staging.Get();
  SDL_GPUTextureRegion to{};
  to.texture = image.Get();
  to.w = to.h = kSide;
  to.d = 1;
  SDL_UploadToGPUTexture(copy, &from, &to, false);
  SDL_EndGPUCopyPass(copy);
  if (!Land(device, commands)) { return {}; }
  return image;
}

struct Output {
  std::array<OwnedTexture, 5> Channels;
  OwnedTexture Depth;
};

Output Targets(SDL_GPUDevice *device) {
  Output out;
  constexpr std::array formats{SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT};
  for (size_t at = 0; at < formats.size(); ++at) {
    out.Channels[at] = Image(device, formats[at], SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
  }
  out.Depth =
      Image(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
  return out;
}

std::vector<float> Depths(SDL_GPUDevice *device, SDL_GPUTexture *image) {
  Readback read;
  if (read.FromTexture(device, image, {.WidthPx = kSide, .HeightPx = kSide}, 4) !=
      ReadState::Ready) {
    return {};
  }
  std::vector<float> values(kSide * kSide);
  std::memcpy(values.data(), read.Rows(), values.size() * sizeof(float));
  return values;
}

void Materials(SDL_GPUDevice *device, const Output &out) {
  Readback identity;
  CHECK(identity.FromTexture(
            device, out.Channels[2].Get(), {.WidthPx = kSide, .HeightPx = kSide}, 16) ==
            ReadState::Ready,
        "reprojected identity reads back");
  std::array<float, 4> id{};
  if (identity.Rows() != nullptr) { std::memcpy(id.data(), identity.Rows() + kCentre * 16, 16); }
  CHECK(id[0] == 7 && id[3] == 1, "reprojection retains the original material identity");
  constexpr std::array<std::array<uint16_t, 4>, 4> expected{{{{0x3400, 0x3800, 0x3a00, 0x3c00}},
                                                             {{0, 0, 0x3c00, 0x3c00}},
                                                             {{0x3400, 0x3800, 0x3a00, 0x3c00}},
                                                             {{0x3000, 0x3800, 0, 0}}}};
  constexpr std::array<size_t, 4> channels{0, 1, 3, 4};
  for (size_t at = 0; at < channels.size(); ++at) {
    const uint32_t bytes = at == 3 ? 4u : 8u;
    Readback channel;
    CHECK(channel.FromTexture(device,
                              out.Channels[channels[at]].Get(),
                              {.WidthPx = kSide, .HeightPx = kSide},
                              bytes) == ReadState::Ready,
          "reprojected native channel reads back");
    std::array<uint16_t, 4> actual{};
    if (channel.Rows() != nullptr) {
      std::memcpy(actual.data(), channel.Rows() + kCentre * bytes, bytes);
    }
    CHECK(
        actual == expected[at],
        "HDR stays untouched and normal, base colour and metallic roughness retain native values");
  }
}

bool Draw(SDL_GPUDevice *device,
          const SurfaceReprojectionStage &stage,
          Output &out,
          const CameraBasis &camera,
          const Lens &lens,
          float occluder = 0) {
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  std::array<SDL_GPUColorTargetInfo, 5> targets{};
  for (size_t at = 0; at < targets.size(); ++at) {
    targets[at].texture = out.Channels[at].Get();
    targets[at].load_op = SDL_GPU_LOADOP_CLEAR;
    targets[at].store_op = SDL_GPU_STOREOP_STORE;
  }
  targets[0].clear_color = {0.25f, 0.5f, 0.75f, 1.0f};
  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture = out.Depth.Get();
  depth.clear_depth = occluder;
  depth.load_op = SDL_GPU_LOADOP_CLEAR;
  depth.store_op = SDL_GPU_STOREOP_STORE;
  depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  auto *pass = SDL_BeginGPURenderPass(
      commands, targets.data(), static_cast<uint32_t>(targets.size()), &depth);
  StageSubmission submission;
  const auto drawn =
      stage.Encode(camera, lens, {.Commands = commands, .Pass = pass, .Submission = submission});
  SDL_EndGPURenderPass(pass);
  if (!drawn) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  return Land(device, commands);
}

void Probe(SDL_GPUDevice *device) {
  std::array<OwnedTexture, 5> inputs{Upload(device, {{0.5f, 0, 0, 0}}, true),
                                     Upload(device, {{0, 0, 1, 1}}, false),
                                     Upload(device, {{7, 0, 0, 1}}, false),
                                     Upload(device, {{0.25f, 0.5f, 0.75f, 1}}, false),
                                     Upload(device, {{0.125f, 0.5f, 0, 0}}, false)};
  CHECK(std::ranges::all_of(inputs, [](const auto &image) { return static_cast<bool>(image); }),
        "captured material and depth channels upload");
  auto out = Targets(device);
  SDL_GPUSamplerCreateInfo sampling{};
  sampling.min_filter = sampling.mag_filter = SDL_GPU_FILTER_NEAREST;
  sampling.address_mode_u = sampling.address_mode_v = sampling.address_mode_w =
      SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  OwnedSampler sampler(device, SDL_CreateGPUSampler(device, &sampling));
  Gpu gpu{.Device = device,
          .HdrFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
          .Width = kSide,
          .Height = kSide};
  for (auto resource : {Resource::SceneHdr,
                        Resource::SceneShadingNormal,
                        Resource::SceneSurfaceIdentity,
                        Resource::SceneSurfaceBase,
                        Resource::SceneSurfaceMetalRough}) {
    CHECK(gpu.SceneColours.Add(resource), "capture targets retain native binding order");
  }
  Lens lens{.WidePx = kSide, .HighPx = kSide, .FovDeg = 90, .NearM = 1};
  SurfaceReprojectionStage::Capture capture{.Projection = lens};
  for (size_t at = 0; at < inputs.size(); ++at) {
    capture.DepthNormalIdentityBaseMetalRough[at] = inputs[at].Get();
  }
  SurfaceReprojectionStage stage;
  CHECK(stage.Configure(gpu, capture, sampler.Get()).has_value(),
        "real reprojection shaders configure");
  CHECK(Draw(device, stage, out, {}, lens), "unchanged view replays the capture");
  auto depths = Depths(device, out.Depth.Get());
  CHECK(depths.size() == kSide * kSide && depths[0] == 0 &&
            std::abs(depths[kCentre] - 0.5f) < 1e-6f,
        "uncovered samples stay empty and covered samples retain geometric depth");
  Materials(device, out);
  CHECK(Draw(device, stage, out, {}, lens, 0.75f), "existing nearer depth participates in replay");
  depths = Depths(device, out.Depth.Get());
  CHECK(depths.size() == kSide * kSide && depths[kCentre] == 0.75f,
        "captured surfaces cannot overwrite nearer native geometry");
  const double angle = 20 * std::numbers::pi / 180;
  auto turn = Viewpoint::LookAt({.EyeM = {}, .AimM = {{std::sin(angle), 0, -std::cos(angle)}}}, 0);
  CHECK(turn && Draw(device, stage, out, *turn, lens), "rotation reuses the same capture position");
  depths = Depths(device, out.Depth.Get());
  CHECK(depths.size() == kSide * kSide && std::abs(depths[kCentre] - 0.5 * std::cos(angle)) < 1e-6,
        "reprojection changes depth to the independently calculated plane intersection");
  Materials(device, out);
  Lens nearer = lens;
  nearer.NearM = 0.25f;
  CHECK(Draw(device, stage, out, {}, nearer),
        "a changed near plane uses the current depth convention");
  depths = Depths(device, out.Depth.Get());
  CHECK(depths.size() == kSide * kSide && std::abs(depths[kCentre] - 0.125f) < 1e-6f,
        "depth is transformed rather than copied between projections");
  CameraBasis opposed;
  opposed.Forward = {{0, 0, 1}};
  opposed.Right = {{-1, 0, 0}};
  CHECK(Draw(device, stage, out, opposed, lens),
        "the opposing camera remains a legal replay request");
  depths = Depths(device, out.Depth.Get());
  CHECK(depths.size() == kSide * kSide &&
            std::ranges::all_of(depths, [](float depth) { return depth == 0; }),
        "a capture behind the camera cannot fill the opposite hemisphere");
  CameraBasis moved;
  moved.EyeM[0] = 0.001;
  CHECK(!Draw(device, stage, out, moved, lens), "movement explicitly retains native geometry");
  Lens invalid = lens;
  invalid.FovDeg = std::numeric_limits<float>::quiet_NaN();
  CHECK(!Draw(device, stage, out, {}, invalid),
        "nonfinite projections are rejected before lens evaluation");
  invalid = lens;
  invalid.OrthoM = 2;
  CHECK(!Draw(device, stage, out, {}, invalid),
        "orthographic sources need a different reprojection contract");
  auto incomplete = capture;
  incomplete.DepthNormalIdentityBaseMetalRough[0] = nullptr;
  CHECK(!stage.Configure(gpu, incomplete, sampler.Get()),
        "incomplete capture channels are rejected");
  CHECK(Draw(device, stage, out, {}, lens),
        "a rejected replacement preserves the last complete capture");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO) && SDL_ShaderCross_Init(),
        "video and shader translator initialize");
  {
    OwnedDevice device(SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(), true, nullptr));
    CHECK(static_cast<bool>(device), "real GPU device initializes");
    if (device) { Probe(device.Get()); }
  }
  SDL_ShaderCross_Quit();
  SDL_Quit();
  return Report();
}
