#include "AirCell.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "ShaderFile.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
constexpr ComputeShape kShape = {.ReadOnlyBuffers = 1, .ReadWriteBuffers = 1, .GroupX = 64};
constexpr size_t kMaximumInputBytes = size_t{64} * 1024 * 1024;
constexpr double kRadianceFloorPerSr = 1e-5;
constexpr double kRadianceRelativeTolerance = 1e-5;
constexpr double kTransmittanceTolerance = 2e-6;

struct Reference {
  double RadiancePerLux;
  double Transmittance;
};

static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(Reference) == 16);

template <typename Value>
std::expected<std::vector<Value>, std::string> ReadBinary(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) { return std::unexpected(std::string(path) + " cannot be opened"); }
  const auto bytes = static_cast<std::streamoff>(file.tellg());
  if (bytes <= 0 || static_cast<size_t>(bytes) > kMaximumInputBytes ||
      bytes % static_cast<std::streamoff>(sizeof(Value)) != 0) {
    return std::unexpected(std::string(path) + " has an invalid record extent");
  }
  std::vector<Value> values(static_cast<size_t>(bytes) / sizeof(Value));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(values.data()), bytes)) {
    return std::unexpected(std::string(path) + " is incomplete");
  }
  return values;
}

bool Compare(std::string_view label,
             std::span<const AirCellChannel> samples,
             std::span<const Reference> expected) {
  size_t failures = 0;
  double worstRadiance = 0.0;
  double worstTransmittance = 0.0;
  for (size_t index = 0; index < samples.size(); ++index) {
    const auto sample = samples[index];
    const auto reference = expected[index];
    const double radiance = std::fabs(sample.RadiancePerLux - reference.RadiancePerLux) /
                            std::max(kRadianceFloorPerSr, std::fabs(reference.RadiancePerLux));
    const double through = std::fabs(sample.Transmittance - reference.Transmittance);
    worstRadiance = std::max(worstRadiance, radiance);
    worstTransmittance = std::max(worstTransmittance, through);
    if (!std::isfinite(sample.RadiancePerLux) || !std::isfinite(sample.Transmittance) ||
        sample.RadiancePerLux < 0.0f || sample.Transmittance < 0.0f ||
        sample.Transmittance > 1.0f || radiance > kRadianceRelativeTolerance ||
        through > kTransmittanceTolerance) {
      if (failures < 8) {
        std::println("{} failed record {}: L={} T={}, expected L={} T={}",
                     label,
                     index,
                     sample.RadiancePerLux,
                     sample.Transmittance,
                     reference.RadiancePerLux,
                     reference.Transmittance);
      }
      ++failures;
    }
  }
  std::println("{} records={} failures={} max_L_relative={} max_T_absolute={}",
               label,
               samples.size(),
               failures,
               worstRadiance,
               worstTransmittance);
  return failures == 0;
}

std::expected<std::vector<AirCellChannel>, std::string>
Run(SDL_GPUDevice *device, SDL_GPUComputePipeline *pipeline, std::span<const AirCellInput> inputs) {
  const auto inputBytes = static_cast<uint32_t>(inputs.size_bytes());
  const auto outputBytes = static_cast<uint32_t>(inputs.size() * sizeof(AirCellChannel));
  SDL_GPUBufferCreateInfo bufferInfo{};
  bufferInfo.size = inputBytes;
  bufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
  const OwnedBuffer source(device, SDL_CreateGPUBuffer(device, &bufferInfo));
  bufferInfo.size = outputBytes;
  bufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
  const OwnedBuffer target(device, SDL_CreateGPUBuffer(device, &bufferInfo));
  SDL_GPUTransferBufferCreateInfo transferInfo{};
  transferInfo.size = inputBytes;
  transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  const OwnedTransfer upload(device, SDL_CreateGPUTransferBuffer(device, &transferInfo));
  if (!source || !target || !upload) { return std::unexpected(std::string(SDL_GetError())); }
  void *mapped = SDL_MapGPUTransferBuffer(device, upload.Get(), false);
  if (mapped == nullptr) { return std::unexpected(std::string(SDL_GetError())); }
  std::memcpy(mapped, inputs.data(), inputBytes);
  SDL_UnmapGPUTransferBuffer(device, upload.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (commands == nullptr) { return std::unexpected(std::string(SDL_GetError())); }
  auto *copy = SDL_BeginGPUCopyPass(commands);
  if (copy == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return std::unexpected(std::string(SDL_GetError()));
  }
  SDL_GPUTransferBufferLocation from{};
  from.transfer_buffer = upload.Get();
  SDL_GPUBufferRegion to{};
  to.buffer = source.Get();
  to.size = inputBytes;
  SDL_UploadToGPUBuffer(copy, &from, &to, false);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUStorageBufferReadWriteBinding into{};
  into.buffer = target.Get();
  auto *compute = SDL_BeginGPUComputePass(commands, nullptr, 0, &into, 1);
  if (compute == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return std::unexpected(std::string(SDL_GetError()));
  }
  SDL_BindGPUComputePipeline(compute, pipeline);
  SDL_GPUBuffer *input = source.Get();
  SDL_BindGPUComputeStorageBuffers(compute, 0, &input, 1);
  SDL_DispatchGPUCompute(
      compute, (static_cast<uint32_t>(inputs.size()) + kShape.GroupX - 1u) / kShape.GroupX, 1, 1);
  SDL_EndGPUComputePass(compute);
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    return std::unexpected(std::string(SDL_GetError()));
  }
  Readback readback;
  if (readback.FromBuffer(device, target.Get(), outputBytes) != ReadState::Ready) {
    return std::unexpected(std::string(SDL_GetError()));
  }
  std::vector<AirCellChannel> samples(inputs.size());
  std::memcpy(samples.data(), readback.Rows(), outputBytes);
  return samples;
}

int OnDevice(const char *shader,
             std::span<const AirCellInput> inputs,
             std::span<const Reference> expected) {
  const OwnedDevice device(SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
      true,
      nullptr));
  if (!device) {
    std::println(stderr, "device: {}", SDL_GetError());
    return 2;
  }
  std::string error;
  const OwnedComputePipeline pipeline(device.Get(),
                                      CompileComputePipeline(device.Get(), shader, kShape, error));
  if (!pipeline) {
    std::println(stderr, "pipeline: {}", error);
    return 2;
  }
  const auto samples = Run(device.Get(), pipeline.Get(), inputs);
  if (!samples) {
    std::println(stderr, "dispatch/readback: {}", samples.error());
    return 2;
  }
  std::println("backend={}; cell mathematics only; no source-LUT or frame-time proof",
               SDL_GetGPUDeviceDriver(device.Get()));
  return Compare("GPU", *samples, expected) ? 0 : 1;
}
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::println(stderr, "usage: air-cell-gpu kernel.spv inputs.bin expected.bin");
    return 2;
  }
  const auto inputs = ReadBinary<outshine::AirCellInput>(argv[2]);
  const auto expected = ReadBinary<Reference>(argv[3]);
  if (!inputs) {
    std::println(stderr, "{}", inputs.error());
    return 2;
  }
  if (!expected) {
    std::println(stderr, "{}", expected.error());
    return 2;
  }
  if (inputs->size() != expected->size()) {
    std::println(stderr, "input and expected counts differ");
    return 2;
  }
  std::vector<outshine::AirCellChannel> cpu;
  cpu.reserve(inputs->size());
  for (const auto &input : *inputs) { cpu.push_back(outshine::air_cell::airIntegrateCell(input)); }
  if (!Compare("CPU", cpu, *expected)) { return 1; }
  if (!SDL_SetHint(SDL_HINT_ASSERT, "abort") || !SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "SDL: {}", SDL_GetError());
    return 2;
  }
  const int status = OnDevice(argv[1], *inputs, *expected);
  SDL_Quit();
  return status;
}
