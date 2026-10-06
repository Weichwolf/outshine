#include "Check.h"
#include "DepthPyramidStage.h"
#include "GpuOwned.h"
#include "GpuPlacement.h"
#include "Readback.h"
#include "ShaderFile.h"
#include "StageSubmission.h"
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kPlaneComponents = 24;
constexpr size_t kSourceOffset = 244;
constexpr float kOccluderDepth = 0.75f;
constexpr float kSmallSphere = 0.000001f;
constexpr float kCoarseSphere = 0.02f;
constexpr Texels kOddExtent{.WidthPx = 37, .HeightPx = 25};
constexpr Texels k720p{.WidthPx = 1280, .HeightPx = 720};
constexpr Texels k1080p{.WidthPx = 1920, .HeightPx = 1080};

struct Probe {
  uint32_t X;
  uint32_t Y;
  float Radius;
};

constexpr std::array<float, 16> kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

bool Land(SDL_GPUDevice *device, SDL_GPUCommandBuffer *commands) {
  const OwnedFence fence(device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
  const std::array<SDL_GPUFence *, 1> handles{fence.Get()};
  const bool landed = fence && SDL_WaitForGPUFences(device, true, handles.data(), 1);
  CHECK(landed, "real GPU commands complete");
  return landed;
}

OwnedBuffer Upload(SDL_GPUDevice *device, const void *data, uint32_t bytes) {
  const SDL_GPUBufferCreateInfo info{
      .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = bytes, .props = 0};
  OwnedBuffer buffer(device, SDL_CreateGPUBuffer(device, &info));
  const SDL_GPUTransferBufferCreateInfo staging{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = bytes, .props = 0};
  const OwnedTransfer transfer(device, SDL_CreateGPUTransferBuffer(device, &staging));
  CHECK(buffer && transfer, "bounded GPU input buffers allocate");
  if (!buffer || !transfer) { return {}; }
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer.Get(), false);
  CHECK(mapped != nullptr, "input staging maps");
  if (mapped == nullptr) { return {}; }
  std::memcpy(mapped, data, bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  auto *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTransferBufferLocation from{.transfer_buffer = transfer.Get(), .offset = 0};
  const SDL_GPUBufferRegion into{.buffer = buffer.Get(), .offset = 0, .size = bytes};
  SDL_UploadToGPUBuffer(copy, &from, &into, false);
  SDL_EndGPUCopyPass(copy);
  if (!Land(device, commands)) { return {}; }
  return buffer;
}

struct alignas(16) CullInput {
  std::array<float, kPlaneComponents> Planes{1, 0,  0, 1, -1, 0, 0, 1, 0, 1, 0,  1,
                                             0, -1, 0, 1, 0,  0, 1, 0, 0, 0, -1, 1};
  std::array<float, 4> Shift{};
  uint32_t Jobs = 1;
  float ErrorPerMetre = 0;
  std::array<uint32_t, 2> Pad{};
  std::array<float, 16> Clip = kIdentity;
  std::array<uint32_t, 4> Wide{}, High{}, At{};
  uint32_t Occludes = 1;
  std::array<uint32_t, 3> Source{};
};

static_assert(sizeof(CullInput) == 256 && offsetof(CullInput, Source) == kSourceOffset);

uint32_t Selected(
    SDL_GPUDevice *device, SDL_GPUBuffer *pyramid, PyramidShape shape, Texels extent, Probe probe) {
  const auto width = extent.WidthPx;
  const auto height = extent.HeightPx;
  const auto x = probe.X;
  const auto y = probe.Y;
  const auto radius = probe.Radius;
  const float px = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(width) - 1.0f;
  const float py = 1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
  const std::array<float, 12> spheres{
      px, py, 0.5f, radius, px, py, 0.5f, radius, 0, std::numeric_limits<float>::infinity(), 0, 0};
  constexpr std::array<uint32_t, 4> jobs{0, 0, 0, 3};
  GpuPlacement placement;
  placement.Current = kIdentity;
  constexpr std::array<uint32_t, 5> arguments{3, 1, 0, 0, 0};
  const auto bounds = Upload(device, spheres.data(), sizeof(spheres));
  const auto work = Upload(device, jobs.data(), sizeof(jobs));
  const auto model = Upload(device, &placement, sizeof(placement));
  const auto args = Upload(device, arguments.data(), sizeof(arguments));
  const SDL_GPUBufferCreateInfo output{
      .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, .size = sizeof(uint32_t), .props = 0};
  const OwnedBuffer kept(device, SDL_CreateGPUBuffer(device, &output));
  auto pipeline = CreateComputePipeline(device, ComputeShaderId::SubjectCull);
  CHECK(bounds && work && model && args && kept && pipeline, "actual cull inputs configure");
  if (!bounds || !work || !model || !args || !kept || !pipeline) { return ~uint32_t{0}; }
  CullInput view;
  view.Wide = shape.Wide;
  view.High = shape.High;
  view.At = shape.At;
  view.Source = {width, height, 0};
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  const SDL_GPUStorageBufferReadWriteBinding target{.buffer = kept.Get(), .cycle = false};
  auto *pass = SDL_BeginGPUComputePass(commands, nullptr, 0, &target, 1);
  SDL_BindGPUComputePipeline(pass, pipeline->Get());
  const std::array<SDL_GPUBuffer *, 5> reads{
      bounds.Get(), work.Get(), model.Get(), args.Get(), pyramid};
  SDL_BindGPUComputeStorageBuffers(pass, 0, reads.data(), static_cast<uint32_t>(reads.size()));
  SDL_PushGPUComputeUniformData(commands, 0, &view, sizeof(view));
  SDL_DispatchGPUCompute(pass, 1, 1, 1);
  SDL_EndGPUComputePass(pass);
  if (!Land(device, commands)) { return ~uint32_t{0}; }
  Readback result;
  CHECK(result.FromBuffer(device, kept.Get(), sizeof(uint32_t)) == ReadState::Ready,
        "actual culling output is readable");
  uint32_t selected = ~uint32_t{0};
  if (result.Rows() != nullptr) { std::memcpy(&selected, result.Rows(), sizeof(selected)); }
  return selected;
}

void CheckExtent(SDL_GPUDevice *device, Texels extent, bool border) {
  const auto width = extent.WidthPx;
  const auto height = extent.HeightPx;
  std::vector<float> pixels(static_cast<size_t>(width) * height, kOccluderDepth);
  if (border) {
    for (uint32_t x = 0; x < width; ++x) { pixels[(height - 1u) * width + x] = 0; }
  } else {
    pixels[(height / 2u) * width + width - 2u] = 0;
  }
  SDL_GPUTextureCreateInfo image{};
  image.type = SDL_GPU_TEXTURETYPE_2D;
  image.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
  image.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  image.width = width;
  image.height = height;
  image.layer_count_or_depth = image.num_levels = 1;
  const OwnedTexture texture(device, SDL_CreateGPUTexture(device, &image));
  const auto bytes = static_cast<uint32_t>(pixels.size() * sizeof(float));
  const SDL_GPUTransferBufferCreateInfo staging{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = bytes, .props = 0};
  const OwnedTransfer transfer(device, SDL_CreateGPUTransferBuffer(device, &staging));
  CHECK(texture && transfer, "depth-valued scalar texture allocates");
  if (!texture || !transfer) { return; }
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer.Get(), false);
  CHECK(mapped != nullptr, "scalar depth staging maps");
  if (mapped == nullptr) { return; }
  std::memcpy(mapped, pixels.data(), bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  auto *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTextureTransferInfo from{.transfer_buffer = transfer.Get(),
                                        .offset = 0,
                                        .pixels_per_row = width,
                                        .rows_per_layer = height};
  const SDL_GPUTextureRegion into{.texture = texture.Get(),
                                  .mip_level = 0,
                                  .layer = 0,
                                  .x = 0,
                                  .y = 0,
                                  .z = 0,
                                  .w = width,
                                  .h = height,
                                  .d = 1};
  SDL_UploadToGPUTexture(copy, &from, &into, false);
  SDL_EndGPUCopyPass(copy);
  if (!Land(device, commands)) { return; }
  const PyramidShape shape = PyramidOver({.WidthPx = width, .HeightPx = height});
  const SDL_GPUBufferCreateInfo output{.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                                SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
                                       .size = shape.Texels * static_cast<uint32_t>(sizeof(float)),
                                       .props = 0};
  const OwnedBuffer pyramid(device, SDL_CreateGPUBuffer(device, &output));
  const SDL_GPUSamplerCreateInfo sample{};
  const OwnedSampler sampler(device, SDL_CreateGPUSampler(device, &sample));
  DepthPyramidStage stage;
  std::string error;
  CHECK(stage.Configure({.Device = device},
                        texture.Get(),
                        sampler.Get(),
                        pyramid.Get(),
                        {.WidthPx = static_cast<int>(width), .HeightPx = static_cast<int>(height)},
                        error),
        "real depth reduction configures");
  if (!stage.Stands()) { return; }
  commands = SDL_AcquireGPUCommandBuffer(device);
  const SDL_GPUStorageBufferReadWriteBinding reduced{.buffer = pyramid.Get(), .cycle = false};
  auto *pass = SDL_BeginGPUComputePass(commands, nullptr, 0, &reduced, 1);
  StageSubmission submission;
  stage.Encode({.Commands = commands, .Pass = nullptr, .Dispatch = pass, .Submission = submission});
  SDL_EndGPUComputePass(pass);
  if (!Land(device, commands)) { return; }
  Readback read;
  CHECK(read.FromBuffer(device, pyramid.Get(), output.size) == ReadState::Ready,
        "reduced depth is read from the GPU");
  if (read.Rows() == nullptr) { return; }
  const auto *values = reinterpret_cast<const float *>(read.Rows());
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    const uint32_t block = 2u << level;
    CHECK(shape.Wide[level] == (width + block - 1u) / block &&
              shape.High[level] == (height + block - 1u) / block,
          "every source pixel belongs to a depth block at every level");
    if (border) {
      const auto lastRow = shape.At[level] + (shape.High[level] - 1u) * shape.Wide[level];
      CHECK(values[lastRow] == 0, "uncovered last source row reaches the last reduced block");
    }
  }
  const uint32_t x = border ? width / 2u : width - 2u;
  const uint32_t y = border ? height - 1u : height / 2u;
  CHECK(Selected(device, pyramid.Get(), shape, extent, {.X = x, .Y = y, .Radius = kSmallSphere}) ==
            3,
        "a genuine uncovered pixel cannot hide its cluster");
  if (border) {
    CHECK(Selected(
              device, pyramid.Get(), shape, extent, {.X = x, .Y = y, .Radius = kCoarseSphere}) == 3,
          "coarser reduction also preserves uncovered edge coverage");
  }
  CHECK(Selected(device,
                 pyramid.Get(),
                 shape,
                 extent,
                 {.X = width / 2u, .Y = height / 2u, .Radius = kSmallSphere}) == 0,
        "fully covered interior still rejects the hidden cluster");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO) && SDL_ShaderCross_Init(), "GPU test services initialize");
  {
    const OwnedDevice device(
        SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(), true, nullptr));
    CHECK(device, "a real GPU is required for depth coverage");
    if (device) {
      CheckExtent(device.Get(), kOddExtent, true);
      CheckExtent(device.Get(), kOddExtent, false);
      CheckExtent(device.Get(), k720p, true);
      CheckExtent(device.Get(), k1080p, true);
    }
  }
  SDL_ShaderCross_Quit();
  SDL_Quit();
  return Report();
}
