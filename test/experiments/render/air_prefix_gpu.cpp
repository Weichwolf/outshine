#include "Atmosphere.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "ShaderFile.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
constexpr ComputeShape kShape = {
    .Samplers = 2, .ReadOnlyBuffers = 2, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64};
constexpr size_t kMaximumFileBytes = size_t{128} * 1024 * 1024;
using uint = uint32_t;
#include "AirRayLayout.h"
#include "AirPrefixProbeLayout.h"
constexpr uint32_t kRayBytes = sizeof(ProbeRay);
constexpr uint32_t kSampleBytes = sizeof(ProbeSample);
constexpr uint32_t kPixelBytes = 4 * sizeof(float);
static_assert(std::endian::native == std::endian::little);
static_assert(std::is_trivially_copyable_v<ProbeRay> && sizeof(ProbeRay) == 32);
static_assert(sizeof(AirRay) == 5 * sizeof(float) &&
              offsetof(ProbeRay, QueryBegin) == sizeof(AirRay));
static_assert(offsetof(ProbeRay, QueryEnd) == 6 * sizeof(float) &&
              offsetof(ProbeRay, Pad) == 7 * sizeof(float));
static_assert(std::is_trivially_copyable_v<ProbeSample> && sizeof(ProbeSample) == 32);
static_assert(offsetof(ProbeSample, ThroughR) == 12 &&
              offsetof(ProbeSample, SourceEvaluations) == 6 * sizeof(float) &&
              offsetof(ProbeSample, Pad) == 7 * sizeof(float));

std::vector<std::byte> Read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = static_cast<std::streamoff>(file.tellg());
  if (!file || bytes <= 0 || static_cast<size_t>(bytes) > kMaximumFileBytes) { return {}; }
  std::vector<std::byte> data(static_cast<size_t>(bytes));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(data.data()), bytes)) { return {}; }
  return data;
}

struct BufferSpec {
  uint32_t Bytes;
  SDL_GPUBufferUsageFlags Usage;
};

struct TextureExtent {
  uint32_t WidthPx;
  uint32_t HeightPx;
};

OwnedBuffer Buffer(SDL_GPUDevice *device, BufferSpec spec) {
  SDL_GPUBufferCreateInfo info{};
  info.size = spec.Bytes;
  info.usage = spec.Usage;
  return {device, SDL_CreateGPUBuffer(device, &info)};
}

OwnedTexture Texture(SDL_GPUDevice *device, TextureExtent extent) {
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = extent.WidthPx;
  info.height = extent.HeightPx;
  info.layer_count_or_depth = info.num_levels = 1;
  return {device, SDL_CreateGPUTexture(device, &info)};
}

OwnedTransfer Upload(SDL_GPUDevice *device, std::span<const std::byte> bytes) {
  SDL_GPUTransferBufferCreateInfo info{};
  info.size = static_cast<uint32_t>(bytes.size());
  info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  OwnedTransfer transfer(device, SDL_CreateGPUTransferBuffer(device, &info));
  if (!transfer) { return {}; }
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer.Get(), false);
  if (mapped == nullptr) { return {}; }
  std::memcpy(mapped, bytes.data(), bytes.size());
  SDL_UnmapGPUTransferBuffer(device, transfer.Get());
  return transfer;
}

struct Data {
  std::array<std::vector<std::byte>, 5> Sources;
  std::array<TextureExtent, 2> Extents{};
  uint32_t RayCount = 0;
  uint32_t QueryCount = 0;
  Medium Air{};

  bool QueriesValid() const {
    const auto &rays = Sources[0];
    const auto &queries = Sources[1];
    uint32_t previousEnd = 0;
    for (size_t offset = 0; offset < rays.size(); offset += kRayBytes) {
      ProbeRay ray{};
      std::memcpy(&ray, rays.data() + offset, sizeof(ray));
      if (ray.QueryBegin != previousEnd || ray.QueryBegin > ray.QueryEnd ||
          ray.QueryEnd > QueryCount || !std::isfinite(ray.Ray.RadiusKm) ||
          ray.Ray.RadiusKm < Air.BottomRadiusKm || ray.Ray.RadiusKm > Air.TopRadiusKm ||
          !std::isfinite(ray.Ray.DistanceKm) || ray.Ray.DistanceKm < 0.0f ||
          !(std::fabs(ray.Ray.CosView) <= 1.0f) || !(std::fabs(ray.Ray.CosSun) <= 1.0f) ||
          !(std::fabs(ray.Ray.ViewSun) <= 1.0f)) {
        return false;
      }
      float previousKm = 0.0f;
      for (uint32_t index = ray.QueryBegin; index < ray.QueryEnd; ++index) {
        float distanceKm = 0.0f;
        std::memcpy(&distanceKm, queries.data() + size_t{index} * sizeof(float), sizeof(float));
        if (!(distanceKm >= previousKm && distanceKm <= ray.Ray.DistanceKm)) { return false; }
        previousKm = distanceKm;
      }
      previousEnd = ray.QueryEnd;
    }
    return previousEnd == QueryCount;
  }

  bool Load(char **paths) {
    for (size_t index = 0; index < Sources.size(); ++index) { Sources[index] = Read(paths[index]); }
    for (size_t index = 0; index < Extents.size(); ++index) {
      auto &pixels = Sources[index + 2];
      if (pixels.size() < sizeof(TextureExtent)) { return false; }
      std::memcpy(&Extents[index], pixels.data(), sizeof(TextureExtent));
      const auto extent = Extents[index];
      constexpr uint32_t kMaximumDimension = 2048;
      if (extent.WidthPx == 0 || extent.HeightPx == 0 || extent.WidthPx > kMaximumDimension ||
          extent.HeightPx > kMaximumDimension ||
          pixels.size() !=
              sizeof(TextureExtent) + size_t{extent.WidthPx} * extent.HeightPx * kPixelBytes) {
        return false;
      }
      pixels.erase(pixels.begin(), pixels.begin() + sizeof(TextureExtent));
    }
    const auto &rays = Sources[0];
    const auto &queries = Sources[1];
    if (rays.empty() || rays.size() % kRayBytes != 0 || queries.empty() ||
        queries.size() % sizeof(float) != 0 || Sources[4].size() != sizeof(Medium)) {
      return false;
    }
    std::memcpy(&Air, Sources[4].data(), sizeof(Air));
    RayCount = static_cast<uint32_t>(rays.size() / kRayBytes);
    QueryCount = static_cast<uint32_t>(queries.size() / sizeof(float));
    return QueriesValid();
  }
};

struct Inputs {
  OwnedBuffer Rays, Queries, Output;
  OwnedTexture Veil, Multi;
  OwnedSampler Linear;
  std::array<OwnedTransfer, 4> Transfers;

  Inputs(SDL_GPUDevice *device, const Data &data)
      : Rays(Buffer(device,
                    {.Bytes = static_cast<uint32_t>(data.Sources[0].size()),
                     .Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ})),
        Queries(Buffer(device,
                       {.Bytes = static_cast<uint32_t>(data.Sources[1].size()),
                        .Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ})),
        Output(Buffer(device,
                      {.Bytes = data.QueryCount * kSampleBytes,
                       .Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE})),
        Veil(Texture(device, data.Extents[0])),
        Multi(Texture(device, data.Extents[1])) {
    SDL_GPUSamplerCreateInfo info{};
    info.address_mode_u = info.address_mode_v = info.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
    Linear = {device, SDL_CreateGPUSampler(device, &info)};
    for (size_t index = 0; index < Transfers.size(); ++index) {
      Transfers[index] = Upload(device, data.Sources[index]);
    }
  }

  [[nodiscard]] bool Ready() const {
    if (!Rays || !Queries || !Output || !Veil || !Multi || !Linear) { return false; }
    return std::ranges::all_of(Transfers,
                               [](const auto &transfer) { return static_cast<bool>(transfer); });
  }

  bool Copy(SDL_GPUCommandBuffer *commands, const Data &data) const {
    auto *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == nullptr) { return false; }
    const std::array<SDL_GPUBuffer *, 2> buffers = {Rays.Get(), Queries.Get()};
    for (size_t index = 0; index < buffers.size(); ++index) {
      SDL_GPUTransferBufferLocation source{};
      source.transfer_buffer = Transfers[index].Get();
      SDL_GPUBufferRegion target{};
      target.buffer = buffers[index];
      target.size = static_cast<uint32_t>(data.Sources[index].size());
      SDL_UploadToGPUBuffer(copy, &source, &target, false);
    }
    const std::array<SDL_GPUTexture *, 2> textures = {Veil.Get(), Multi.Get()};
    for (size_t index = 0; index < textures.size(); ++index) {
      SDL_GPUTextureTransferInfo source{};
      source.transfer_buffer = Transfers[index + buffers.size()].Get();
      SDL_GPUTextureRegion target{};
      target.texture = textures[index];
      target.w = data.Extents[index].WidthPx;
      target.h = data.Extents[index].HeightPx;
      target.d = 1;
      SDL_UploadToGPUTexture(copy, &source, &target, false);
    }
    SDL_EndGPUCopyPass(copy);
    return true;
  }
};

bool Dispatch(SDL_GPUComputePipeline *pipeline,
              const Inputs &inputs,
              const Data &data,
              SDL_GPUCommandBuffer *commands) {
  SDL_GPUStorageBufferReadWriteBinding target{};
  target.buffer = inputs.Output.Get();
  auto *compute = SDL_BeginGPUComputePass(commands, nullptr, 0, &target, 1);
  if (compute == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  SDL_BindGPUComputePipeline(compute, pipeline);
  const std::array<SDL_GPUTextureSamplerBinding, 2> textures = {
      SDL_GPUTextureSamplerBinding{inputs.Veil.Get(), inputs.Linear.Get()},
      SDL_GPUTextureSamplerBinding{inputs.Multi.Get(), inputs.Linear.Get()}};
  SDL_BindGPUComputeSamplers(compute, 0, textures.data(), static_cast<uint32_t>(textures.size()));
  std::array<SDL_GPUBuffer *, 2> buffers = {inputs.Rays.Get(), inputs.Queries.Get()};
  SDL_BindGPUComputeStorageBuffers(
      compute, 0, buffers.data(), static_cast<uint32_t>(buffers.size()));
  SDL_PushGPUComputeUniformData(commands, 0, &data.Air, sizeof(data.Air));
  SDL_DispatchGPUCompute(compute, (data.RayCount + kShape.GroupX - 1u) / kShape.GroupX, 1, 1);
  SDL_EndGPUComputePass(compute);
  return true;
}

bool ResidentRuns(SDL_GPUDevice *device,
                  SDL_GPUComputePipeline *pipeline,
                  const Inputs &inputs,
                  const Data &data,
                  uint32_t repeats) {
  std::vector<double> times;
  for (uint32_t index = 0; index < repeats; ++index) {
    auto *commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == nullptr) { return false; }
    const auto start = std::chrono::steady_clock::now();
    if (!Dispatch(pipeline, inputs, data, commands)) { return false; }
    const OwnedFence fence(device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
    if (!fence) { return false; }
    SDL_GPUFence *handle = fence.Get();
    if (!SDL_WaitForGPUFences(device, true, &handle, 1)) { return false; }
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
    times.push_back(elapsed.count());
  }
  if (!times.empty()) {
    std::ranges::sort(times);
    constexpr size_t kP95Numerator = 19;
    constexpr size_t kPercentileDenominator = 20;
    std::println("resident_dispatch_submit_fence_wall_ms samples={} p50={} p95={} maximum={}; "
                 "CPU/GPU queue wall, not GPU timestamps or scene frames",
                 times.size(),
                 times[times.size() / 2],
                 times[times.size() * kP95Numerator / kPercentileDenominator],
                 times.back());
  }
  return true;
}

int Run(SDL_GPUDevice *device,
        const char *shader,
        const Data &data,
        const char *output,
        uint32_t repeats) {
  std::string error;
  const OwnedComputePipeline pipeline(device,
                                      CompileComputePipeline(device, shader, kShape, error));
  if (!pipeline) {
    std::println(stderr, "pipeline: {}", error);
    return 2;
  }
  const Inputs inputs(device, data);
  if (!inputs.Ready()) { return 2; }
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (commands == nullptr) { return 2; }
  if (!inputs.Copy(commands, data)) {
    SDL_CancelGPUCommandBuffer(commands);
    return 2;
  }
  const auto start = std::chrono::steady_clock::now();
  if (!Dispatch(pipeline.Get(), inputs, data, commands) || !SDL_SubmitGPUCommandBuffer(commands)) {
    return 2;
  }
  Readback readback;
  const uint32_t bytes = data.QueryCount * kSampleBytes;
  if (readback.FromBuffer(device, inputs.Output.Get(), bytes) != ReadState::Ready) { return 2; }
  const auto wall =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
  std::ofstream file(output, std::ios::binary);
  file.write(reinterpret_cast<const char *>(readback.Rows()), bytes);
  if (!file) { return 2; }
  std::println("backend={} rays={} queries={} submit_copy_compute_readback_wall_ms={}",
               SDL_GetGPUDeviceDriver(device),
               data.RayCount,
               data.QueryCount,
               wall.count());
  return ResidentRuns(device, pipeline.Get(), inputs, data, repeats) ? 0 : 2;
}
}

int main(int argc, char **argv) {
  if (argc != 8 && argc != 9) {
    std::println(stderr,
                 "usage: air-prefix-gpu kernel.spv rays.bin queries.bin veil.bin multi.bin "
                 "medium.bin output.bin [resident-repeats]");
    return 2;
  }
  uint32_t repeats = 0;
  if (argc == 9) {
    constexpr uint32_t kMaximumRepeats = 120;
    const char *last = argv[8] + std::strlen(argv[8]);
    const auto parsed = std::from_chars(argv[8], last, repeats);
    if (parsed.ec != std::errc{} || parsed.ptr != last || repeats > kMaximumRepeats) { return 2; }
  }
  Data data;
  if (!data.Load(argv + 2)) {
    std::println(stderr, "invalid fixture extents or query spans");
    return 2;
  }
  if (!SDL_SetHint(SDL_HINT_ASSERT, "abort") || !SDL_Init(SDL_INIT_VIDEO)) { return 2; }
  int status = 2;
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
        true,
        nullptr));
    if (device) { status = Run(device.Get(), argv[1], data, argv[7], repeats); }
    if (status != 0) { std::println(stderr, "SDL: {}", SDL_GetError()); }
  }
  SDL_Quit();
  return status;
}
