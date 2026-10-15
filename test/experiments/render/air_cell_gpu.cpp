#include "AirCell.h"
#include "render/device/native/Device.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <utility>
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
namespace gpu = outshine::Render::Native;
constexpr uint32_t kGroupWidth = 64;
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
  if (bytes <= 0 || std::cmp_greater(bytes, kMaximumInputBytes) ||
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

std::expected<std::vector<AirCellChannel>, std::string> Run(gpu::Device &device,
                                                            const gpu::ComputePipeline &pipeline,
                                                            std::span<const AirCellInput> inputs) {
  const uint64_t outputBytes = inputs.size() * sizeof(AirCellChannel);
  constexpr auto kReadUse = gpu::BufferUse::CopyDestination | gpu::BufferUse::Storage;
  constexpr auto kWriteUse = gpu::BufferUse::CopySource | gpu::BufferUse::Storage;
  const std::array<gpu::BufferDescription, 4> descriptions = {{
      {.Bytes = inputs.size_bytes(), .Use = kReadUse},
      {.Bytes = outputBytes, .Use = kWriteUse},
      {.Bytes = inputs.size_bytes(),
       .Use = gpu::BufferUse::CopySource,
       .Placement = gpu::Memory::Upload},
      {.Bytes = outputBytes,
       .Use = gpu::BufferUse::CopyDestination,
       .Placement = gpu::Memory::Readback},
  }};
  std::array<std::unique_ptr<gpu::Buffer>, 4> buffers;
  for (size_t index = 0; index < buffers.size(); ++index) {
    auto buffer = device.CreateBuffer(descriptions[index]);
    if (!buffer) { return std::unexpected(buffer.error().Message); }
    buffers[index] = std::move(*buffer);
  }
  auto mapped = device.MapWrite(*buffers[2], {.Bytes = inputs.size_bytes()});
  if (!mapped) { return std::unexpected(mapped.error().Message); }
  std::memcpy(mapped->data(), inputs.data(), inputs.size_bytes());
  const auto unmapped = device.Unmap(*buffers[2]);
  if (!unmapped) { return std::unexpected(unmapped.error().Message); }
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error().Message); }
  const auto copied = (*commands)->Copy(*buffers[2], *buffers[0], {.Bytes = inputs.size_bytes()});
  if (!copied) { return std::unexpected(copied.error().Message); }
  const std::array<gpu::Binding, 2> bindings = {{
      {.Set = 0,
       .Slot = 0,
       .Value =
           gpu::StorageBuffer{.Data = buffers[0].get(), .Range = {.Bytes = inputs.size_bytes()}}},
      {.Set = 1,
       .Slot = 0,
       .Value = gpu::StorageBuffer{.Data = buffers[1].get(), .Range = {.Bytes = outputBytes}}},
  }};
  const auto dispatched = (*commands)->Dispatch(
      pipeline,
      bindings,
      {.X = (static_cast<uint32_t>(inputs.size()) + kGroupWidth - 1) / kGroupWidth});
  if (!dispatched) { return std::unexpected(dispatched.error().Message); }
  const auto readback = (*commands)->Copy(*buffers[1], *buffers[3], {.Bytes = outputBytes});
  if (!readback) { return std::unexpected(readback.error().Message); }
  const auto submitted = device.Submit(std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error().Message); }
  const auto status = device.Wait(*submitted, std::chrono::seconds(30));
  if (!status) { return std::unexpected(status.error().Message); }
  if (*status != gpu::CompletionStatus::Ready) {
    return std::unexpected("cell probe did not complete within its bounded offline wait");
  }
  const auto downloaded = device.MapRead(*buffers[3], {.Bytes = outputBytes});
  if (!downloaded) { return std::unexpected(downloaded.error().Message); }
  std::vector<AirCellChannel> samples(inputs.size());
  std::memcpy(samples.data(), downloaded->data(), static_cast<size_t>(outputBytes));
  const auto released = device.Unmap(*buffers[3]);
  if (!released) { return std::unexpected(released.error().Message); }
  return samples;
}

int OnDevice(const char *shader,
             std::span<const AirCellInput> inputs,
             std::span<const Reference> expected) {
  const auto device = gpu::CreateDevice({.Validation = true});
  if (!device) {
    std::println(stderr, "device: {}", device.error().Message);
    return 2;
  }
  const auto spirv = ReadBinary<uint32_t>(shader);
  if (!spirv) {
    std::println(stderr, "SPIR-V: {}", spirv.error());
    return 2;
  }
  const auto pipeline = (*device)->CreateComputePipeline(*spirv);
  if (!pipeline) {
    std::println(stderr, "pipeline: {}", pipeline.error().Message);
    return 2;
  }
  const auto &layout = (*pipeline)->Layout();
  if (layout.LocalSize != std::array<uint32_t, 3>{kGroupWidth, 1, 1} ||
      layout.Bindings.size() != 2) {
    std::println(stderr, "cell shader needs two bindings and LocalSize64");
    return 2;
  }
  const auto samples = Run(**device, **pipeline, inputs);
  if (!samples) {
    std::println(stderr, "dispatch/readback: {}", samples.error());
    return 2;
  }
  std::println("device={}; cell mathematics only; no source-LUT or frame-time proof",
               (*device)->Limits().Name);
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
  return OnDevice(argv[1], *inputs, *expected);
}
