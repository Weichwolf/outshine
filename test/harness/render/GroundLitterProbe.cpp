#include "GpuOwned.h"
#include "GroundPalette.h"
#include "VegetationTemplates.h"

#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;

constexpr Uint32 kThreadCount = 64;
constexpr std::streamoff kMaximumInputBytes = 64L * 1024L * 1024L;
constexpr size_t kWhichOffsetBytes = 24, kModeOffsetBytes = 28;
constexpr Vec4f kFallback{{.11f, .22f, .33f, .9f}};

struct alignas(16) Probe {
  std::array<float, 4> PointDx;
  std::array<float, 2> Dy;
  int32_t Which;
  uint32_t Mode;
};

struct alignas(16) Result {
  std::array<float, 4> Wearing, Detail;
};

struct alignas(16) Pushed {
  uint32_t Count, Unmapped;
  std::array<uint32_t, 2> Padding;
};

static_assert(sizeof(Probe) == 32 && alignof(Probe) == 16 && offsetof(Probe, Dy) == 16 &&
              offsetof(Probe, Which) == kWhichOffsetBytes &&
              offsetof(Probe, Mode) == kModeOffsetBytes);
static_assert(sizeof(Result) == 32 && alignof(Result) == 16 && offsetof(Result, Detail) == 16);
static_assert(sizeof(Pushed) == 16 && alignof(Pushed) == 16 && offsetof(Pushed, Unmapped) == 4);

struct Resources {
  OwnedDevice Device;
  OwnedComputePipeline Pipeline;
  OwnedBuffer Palette, Probes, Results;
  OwnedTransfer Upload, Download;
};

std::vector<char> Read(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() <= 0 || input.tellg() > kMaximumInputBytes) { return {}; }
  std::vector<char> bytes(static_cast<size_t>(input.tellg()));
  input.seekg(0);
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return input ? bytes : std::vector<char>{};
}

bool Write(const std::string &path, const void *data, size_t bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(static_cast<const char *>(data), static_cast<std::streamsize>(bytes));
  return static_cast<bool>(output);
}

OwnedBuffer Buffer(SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage, Uint32 bytes) {
  const SDL_GPUBufferCreateInfo info{.usage = usage, .size = bytes, .props = 0};
  return {device, SDL_CreateGPUBuffer(device, &info)};
}

OwnedTransfer Transfer(SDL_GPUDevice *device, SDL_GPUTransferBufferUsage usage, Uint32 bytes) {
  const SDL_GPUTransferBufferCreateInfo info{.usage = usage, .size = bytes, .props = 0};
  return {device, SDL_CreateGPUTransferBuffer(device, &info)};
}

bool Configure(Resources &gpu,
               const std::vector<char> &code,
               Uint32 paletteBytes,
               Uint32 probeBytes) {
  gpu.Device = OwnedDevice(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan"));
  if (!gpu.Device) { return false; }
  SDL_GPUComputePipelineCreateInfo pipeline{};
  pipeline.code_size = code.size();
  pipeline.code = reinterpret_cast<const Uint8 *>(code.data());
  pipeline.entrypoint = "main";
  pipeline.format = SDL_GPU_SHADERFORMAT_SPIRV;
  pipeline.num_readonly_storage_buffers = 2;
  pipeline.num_readwrite_storage_buffers = 1;
  pipeline.num_uniform_buffers = 1;
  pipeline.threadcount_x = kThreadCount;
  pipeline.threadcount_y = pipeline.threadcount_z = 1;
  auto *device = gpu.Device.Get();
  gpu.Pipeline = OwnedComputePipeline(device, SDL_CreateGPUComputePipeline(device, &pipeline));
  gpu.Palette = Buffer(device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, paletteBytes);
  gpu.Probes = Buffer(device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, probeBytes);
  gpu.Results = Buffer(device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, probeBytes);
  gpu.Upload = Transfer(device, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, paletteBytes + probeBytes);
  gpu.Download = Transfer(device, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, probeBytes);
  return gpu.Pipeline && gpu.Palette && gpu.Probes && gpu.Results && gpu.Upload && gpu.Download;
}

bool Capture(Resources &gpu,
             const std::vector<float> &palette,
             const std::vector<char> &probes,
             const Pushed &pushed,
             const char *output) {
  auto *device = gpu.Device.Get();
  const auto paletteBytes = static_cast<Uint32>(palette.size() * sizeof(float));
  const auto probeBytes = static_cast<Uint32>(probes.size());
  void *mapped = SDL_MapGPUTransferBuffer(device, gpu.Upload.Get(), false);
  if (mapped == nullptr) { return false; }
  std::memcpy(mapped, palette.data(), paletteBytes);
  std::memcpy(static_cast<char *>(mapped) + paletteBytes, probes.data(), probeBytes);
  SDL_UnmapGPUTransferBuffer(device, gpu.Upload.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (commands == nullptr) { return false; }
  auto *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTransferBufferLocation paletteFrom{.transfer_buffer = gpu.Upload.Get(), .offset = 0};
  const SDL_GPUTransferBufferLocation probesFrom{.transfer_buffer = gpu.Upload.Get(),
                                                 .offset = paletteBytes};
  const SDL_GPUBufferRegion paletteTo{
      .buffer = gpu.Palette.Get(), .offset = 0, .size = paletteBytes};
  const SDL_GPUBufferRegion probesTo{.buffer = gpu.Probes.Get(), .offset = 0, .size = probeBytes};
  SDL_UploadToGPUBuffer(copy, &paletteFrom, &paletteTo, false);
  SDL_UploadToGPUBuffer(copy, &probesFrom, &probesTo, false);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUStorageBufferReadWriteBinding target{};
  target.buffer = gpu.Results.Get();
  auto *compute = SDL_BeginGPUComputePass(commands, nullptr, 0, &target, 1);
  SDL_BindGPUComputePipeline(compute, gpu.Pipeline.Get());
  const std::array inputs{gpu.Palette.Get(), gpu.Probes.Get()};
  SDL_BindGPUComputeStorageBuffers(compute, 0, inputs.data(), inputs.size());
  SDL_PushGPUComputeUniformData(commands, 0, &pushed, sizeof(pushed));
  SDL_DispatchGPUCompute(compute, (pushed.Count + kThreadCount - 1) / kThreadCount, 1, 1);
  SDL_EndGPUComputePass(compute);
  copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUBufferRegion source{.buffer = gpu.Results.Get(), .offset = 0, .size = probeBytes};
  const SDL_GPUTransferBufferLocation destination{.transfer_buffer = gpu.Download.Get(),
                                                  .offset = 0};
  SDL_DownloadFromGPUBuffer(copy, &source, &destination);
  SDL_EndGPUCopyPass(copy);
  const OwnedFence fence(device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
  if (!fence) { return false; }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!SDL_QueryGPUFence(device, fence.Get())) {
    if (std::chrono::steady_clock::now() >= deadline) { return false; }
    SDL_Delay(1);
  }
  mapped = SDL_MapGPUTransferBuffer(device, gpu.Download.Get(), false);
  if (mapped == nullptr) { return false; }
  const bool saved = Write(output, mapped, probeBytes);
  SDL_UnmapGPUTransferBuffer(device, gpu.Download.Get());
  std::println(
      "backend={} probes={} gpu_ms=unmeasured", SDL_GetGPUDeviceDriver(device), pushed.Count);
  return saved;
}

bool Run(char **argv) {
  const auto code = Read(argv[1]);
  const auto probes = Read(argv[2]);
  if (code.empty() || probes.empty() || probes.size() % sizeof(Probe) != 0) { return false; }
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates templates;
  if (!materials.Load("src/assets/world/ground-materials.json") ||
      !templates.Load("src/assets/world/vegetation.json", materials)) {
    return false;
  }
  auto palette = GroundMaterialPalette(templates, kFallback);
  const size_t rows = templates.TemplateCount();
  const size_t surfacesAt = 4 * (rows + 2) + rows + 1;
  const auto layersAt = std::bit_cast<uint32_t>(palette[3]);
  const std::string_view variant(argv[4]);
  if (variant == "legacy") {
    palette[3] = 0.0f;
  } else if (variant == "neutral") {
    for (size_t row = 0; row < rows; ++row) {
      palette[surfacesAt + 8 * row + 3] = 0.0f;
      palette[layersAt + 8 * row + 7] = 0.0f;
    }
  } else if (variant != "native") {
    return false;
  }
  if (!Write(std::string(argv[3]) + ".palette", palette.data(), palette.size() * sizeof(float))) {
    return false;
  }
  for (size_t row = 0; row < rows; ++row) {
    std::println("row={} name={}", row, templates.Name(row));
  }
  const Pushed pushed{.Count = static_cast<uint32_t>(probes.size() / sizeof(Probe)),
                      .Unmapped = static_cast<uint32_t>(templates.UnmappedRow()),
                      .Padding = {0, 0}};
  Resources gpu;
  return Configure(gpu,
                   code,
                   static_cast<Uint32>(palette.size() * sizeof(float)),
                   static_cast<Uint32>(probes.size())) &&
         Capture(gpu, palette, probes, pushed, argv[3]);
}
}

int main(int argc, char **argv) {
  if (argc != 5) {
    std::fputs(
        "usage: ground_litter_probe shader.spv probes.bin output.f32 native|legacy|neutral\n",
        stderr);
    return 2;
  }
  SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_VERBOSE);
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "{}", SDL_GetError());
    return 1;
  }
  const bool ready = Run(argv);
  if (!ready) { std::println(stderr, "ground litter probe failed: {}", SDL_GetError()); }
  SDL_Quit();
  return ready ? 0 : 1;
}
