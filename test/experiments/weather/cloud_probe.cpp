#include "CloudLayerUniform.h"
#include "GpuOwned.h"

#include <SDL3/SDL.h>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <ios>
#include <numbers>
#include <print>
#include <ratio>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;

struct alignas(16) Pushed {
  CloudLayerUniform Layer;
  Vec4f Origin, Forward, Right, Up, Extent;
};

static_assert(sizeof(Pushed) == 128 && alignof(Pushed) == 16);
static_assert(offsetof(Pushed, Origin) == 48);
static_assert(offsetof(Pushed, Forward) == 64);
static_assert(offsetof(Pushed, Right) == 80);
static_assert(offsetof(Pushed, Up) == 96);
static_assert(offsetof(Pushed, Extent) == 112);

struct Resources {
  OwnedDevice Device;
  OwnedComputePipeline Pipeline;
  OwnedBuffer Results;
  OwnedTransfer Download;
};

template <typename Value> bool Number(std::string_view text, Value &result) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool Configure(Resources &gpu, const char *shaderPath, Uint32 bytes) {
  gpu.Device = OwnedDevice(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan"));
  if (!gpu.Device) { return false; }
  std::ifstream input(shaderPath, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() <= 0) { return false; }
  std::vector<Uint8> code(static_cast<size_t>(input.tellg()));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(code.size()));
  if (!input) { return false; }
  SDL_GPUComputePipelineCreateInfo pipeline{};
  pipeline.code_size = code.size();
  pipeline.code = code.data();
  pipeline.entrypoint = "main";
  pipeline.format = SDL_GPU_SHADERFORMAT_SPIRV;
  pipeline.num_readwrite_storage_buffers = 1;
  pipeline.num_uniform_buffers = 1;
  pipeline.threadcount_x = pipeline.threadcount_y = 8;
  pipeline.threadcount_z = 1;
  gpu.Pipeline = OwnedComputePipeline(gpu.Device.Get(),
                                      SDL_CreateGPUComputePipeline(gpu.Device.Get(), &pipeline));
  const SDL_GPUBufferCreateInfo buffer{
      .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, .size = bytes, .props = 0};
  gpu.Results = OwnedBuffer(gpu.Device.Get(), SDL_CreateGPUBuffer(gpu.Device.Get(), &buffer));
  const SDL_GPUTransferBufferCreateInfo transfer{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = bytes, .props = 0};
  gpu.Download =
      OwnedTransfer(gpu.Device.Get(), SDL_CreateGPUTransferBuffer(gpu.Device.Get(), &transfer));
  return gpu.Pipeline && gpu.Results && gpu.Download;
}

bool Capture(Resources &gpu, const Pushed &pushed, Uint32 bytes, const char *outputPath) {
  const auto before = std::chrono::steady_clock::now();
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(gpu.Device.Get());
  if (commands == nullptr) { return false; }
  const SDL_GPUStorageBufferReadWriteBinding target{
      .buffer = gpu.Results.Get(), .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
  SDL_GPUComputePass *compute = SDL_BeginGPUComputePass(commands, nullptr, 0, &target, 1);
  SDL_BindGPUComputePipeline(compute, gpu.Pipeline.Get());
  SDL_PushGPUComputeUniformData(commands, 0, &pushed, sizeof pushed);
  SDL_DispatchGPUCompute(compute,
                         (static_cast<Uint32>(pushed.Extent[0]) + 7) / 8,
                         (static_cast<Uint32>(pushed.Extent[1]) + 7) / 8,
                         1);
  SDL_EndGPUComputePass(compute);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUBufferRegion source{.buffer = gpu.Results.Get(), .offset = 0, .size = bytes};
  const SDL_GPUTransferBufferLocation destination{.transfer_buffer = gpu.Download.Get(),
                                                  .offset = 0};
  SDL_DownloadFromGPUBuffer(copy, &source, &destination);
  SDL_EndGPUCopyPass(copy);
  const auto encoded = std::chrono::steady_clock::now();
  const OwnedFence fence(gpu.Device.Get(), SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
  if (!fence) { return false; }
  const auto submitted = std::chrono::steady_clock::now();
  const auto deadline = submitted + std::chrono::seconds(30);
  while (!SDL_QueryGPUFence(gpu.Device.Get(), fence.Get())) {
    if (std::chrono::steady_clock::now() >= deadline) { return false; }
    SDL_Delay(1);
  }
  const auto finished = std::chrono::steady_clock::now();
  const void *mapped = SDL_MapGPUTransferBuffer(gpu.Device.Get(), gpu.Download.Get(), false);
  if (mapped == nullptr) { return false; }
  std::ofstream output(outputPath, std::ios::binary);
  output.write(static_cast<const char *>(mapped), bytes);
  SDL_UnmapGPUTransferBuffer(gpu.Device.Get(), gpu.Download.Get());
  std::println("backend={} encode_ms={:.6f} submit_ms={:.6f} fence_observation_ms={:.6f} bytes={} "
               "gpu_ms=unmeasured",
               SDL_GetGPUDeviceDriver(gpu.Device.Get()),
               std::chrono::duration<double, std::milli>(encoded - before).count(),
               std::chrono::duration<double, std::milli>(submitted - encoded).count(),
               std::chrono::duration<double, std::milli>(finished - submitted).count(),
               bytes);
  return static_cast<bool>(output);
}
}

int main(int argc, char **argv) {
  int width = 0;
  int height = 0;
  int channels = 1;
  double cover = 0.0;
  if ((argc != 6 && argc != 7) || !Number(argv[3], width) || !Number(argv[4], height) ||
      !Number(argv[5], cover) || width < 1 || width > 4096 || height < 1 || height > 4096 ||
      !std::isfinite(cover) || cover < 0.0 || cover > 1.0 ||
      (argc == 7 && !Number(argv[6], channels)) || (channels != 1 && channels != 4)) {
    std::fputs("usage: cloud_probe shader.spv output.f32 width height cover [1|4 channels]\n",
               stderr);
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "{}", SDL_GetError());
    return 1;
  }
  const Uint32 bytes = static_cast<Uint32>(width * height * channels) * sizeof(float);
  bool ready = false;
  {
    Resources gpu;
    if (Configure(gpu, argv[1], bytes)) {
      const CloudLayer layer{.BaseM = 2572.0,
                             .Cover = cover,
                             .WindMs = CloudWindFrom({.FromDeg = 240.0, .SpeedMs = 8.0})};
      constexpr double bearing = 240.0 * std::numbers::pi / 180.0;
      constexpr double pitch = -0.5 * std::numbers::pi / 180.0;
      const auto halfHeight = static_cast<float>(std::tan(33.97 * std::numbers::pi / 360.0));
      const Pushed pushed{
          .Layer = PackCloudLayer(layer, 0.0),
          .Origin = {{0.0f, 1772.0f, 0.0f, 0.0f}},
          .Forward = {{static_cast<float>(std::sin(bearing) * std::cos(pitch)),
                       static_cast<float>(std::sin(pitch)),
                       static_cast<float>(-std::cos(bearing) * std::cos(pitch)),
                       0.0f}},
          .Right = {{static_cast<float>(std::cos(bearing)),
                     0.0f,
                     static_cast<float>(std::sin(bearing)),
                     0.0f}},
          .Up = {{static_cast<float>(-std::sin(bearing) * std::sin(pitch)),
                  static_cast<float>(std::cos(pitch)),
                  static_cast<float>(std::cos(bearing) * std::sin(pitch)),
                  0.0f}},
          .Extent = {{static_cast<float>(width),
                      static_cast<float>(height),
                      halfHeight * static_cast<float>(width) / static_cast<float>(height),
                      halfHeight}}};
      ready = Capture(gpu, pushed, bytes, argv[2]);
    }
  }
  if (!ready) { std::println(stderr, "cloud probe failed: {}", SDL_GetError()); }
  SDL_Quit();
  return ready ? 0 : 1;
}
