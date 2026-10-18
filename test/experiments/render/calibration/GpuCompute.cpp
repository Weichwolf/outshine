#include "GpuSupport.h"
#include "Calibration.h"
#include "Parameters.inc"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Diagnostics::Calibration {
namespace {
struct Buffers {
  std::vector<std::unique_ptr<Native::Buffer>> Inputs;
  std::unique_ptr<Native::Buffer> Output;
  std::unique_ptr<Native::Buffer> Readback;
  std::vector<Native::Binding> Bindings;
};

Native::Result<Buffers> Prepare(Native::Device &device,
                                std::span<const std::span<const std::byte>> inputs,
                                size_t outputBytes) {
  Buffers buffers;
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  for (const auto bytes : inputs) {
    auto buffer = device.CreateBuffer(
        {.Bytes = bytes.size(),
         .Use = Native::BufferUse::Storage | Native::BufferUse::CopyDestination});
    if (!buffer) { return std::unexpected(buffer.error()); }
    const auto uploaded = Upload(device, **commands, **buffer, bytes);
    if (!uploaded) { return std::unexpected(uploaded.error()); }
    buffers.Inputs.push_back(std::move(*buffer));
  }
  auto output = device.CreateBuffer(
      {.Bytes = outputBytes, .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource});
  if (!output) { return std::unexpected(output.error()); }
  buffers.Output = std::move(*output);
  auto readback = device.CreateBuffer({.Bytes = outputBytes,
                                       .Use = Native::BufferUse::CopyDestination,
                                       .Placement = Native::Memory::Readback});
  if (!readback) { return std::unexpected(readback.error()); }
  buffers.Readback = std::move(*readback);
  for (uint32_t slot = 0; slot <= buffers.Inputs.size(); ++slot) {
    const auto *buffer =
        slot < buffers.Inputs.size() ? buffers.Inputs[slot].get() : buffers.Output.get();
    buffers.Bindings.push_back(
        {.Set = 0,
         .Slot = slot,
         .Value = Native::StorageBuffer{.Data = buffer,
                                        .Range = {.Bytes = buffer->Description().Bytes}}});
  }
  const auto finished = Finish(device, std::move(*commands));
  if (!finished) { return std::unexpected(finished.error()); }
  return buffers;
}

Native::Result<Workload> Compute(Native::Device &device,
                                 const Native::ComputePipeline &pipeline,
                                 std::span<const std::span<const std::byte>> inputs,
                                 std::span<const std::byte> expected,
                                 Workload work) {
  auto buffers = Prepare(device, inputs, expected.size());
  if (!buffers) { return std::unexpected(buffers.error()); }
  const auto groups =
      static_cast<uint32_t>((work.Elements + CALIBRATION_LOCAL_SIZE - 1u) / CALIBRATION_LOCAL_SIZE);
  if (groups > device.Limits().MaxWorkgroupCount[0]) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Unsupported,
                                         .Message = "calibration dispatch exceeds device limits"});
  }
  for (size_t round = 0; round <= CALIBRATION_SAMPLES; ++round) {
    auto sample = Measure(device, [&](Native::Commands &commands) {
      return commands.Dispatch(pipeline, buffers->Bindings, {.X = groups});
    });
    if (!sample) { return std::unexpected(sample.error()); }
    const auto same = VerifyBuffer(device, *buffers->Output, *buffers->Readback, expected);
    if (!same) { return std::unexpected(same.error()); }
    if (!*same) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "GPU calibration result differs from complete CPU reference"});
    }
    if (round != 0) { work.Samples.push_back(*sample); }
  }
  work.Checksum = Checksum(expected);
  return work;
}

Native::Result<Workload> Fma(Native::Device &device, const Native::ComputePipeline &pipeline) {
  std::vector<FmaValue> input(CALIBRATION_FMA_ELEMENTS);
  std::vector<FmaValue> expected(input.size());
  for (uint32_t at = 0; at < input.size(); ++at) {
    input[at] = FmaInput(at);
    expected[at] = FmaReference(input[at]);
  }
  Workload work{};
  work.Name = "gpu_fma";
  work.TimeBasis = "submission_gpu_ns";
  work.Elements = input.size();
  work.Iterations = CALIBRATION_FMA_STEPS;
  work.LogicalOperations = work.Elements * work.Iterations * 4u * 2u;
  work.WorkingSetBytes = input.size() * sizeof(FmaValue) * 2u;
  work.Note = "Same four-lane FMA recurrence as CPU; one work submission, complete bitwise "
              "readback outside timing. Missing timestamps stay null.";
  const std::array inputs{std::as_bytes(std::span(input))};
  return Compute(device, pipeline, inputs, std::as_bytes(std::span(expected)), std::move(work));
}

Native::Result<Workload>
Triad(Native::Device &device, const Native::ComputePipeline &pipeline, size_t elements) {
  std::vector<uint32_t> a(elements);
  std::vector<uint32_t> b(elements);
  std::vector<uint32_t> expected(elements);
  for (uint32_t at = 0; at < elements; ++at) {
    a[at] = at * CALIBRATION_MIX_MULTIPLIER;
    b[at] = at ^ CALIBRATION_TRIAD_MASK;
    expected[at] = a[at] + 3u * b[at];
  }
  Workload work{};
  work.Name = elements == 65536 ? "gpu_triad_768k" : "gpu_triad_96m";
  work.TimeBasis = "submission_gpu_ns";
  work.Elements = elements;
  work.Iterations = 1;
  work.WorkingSetBytes = elements * sizeof(uint32_t) * 3u;
  work.LogicalBytes = work.WorkingSetBytes;
  work.Note = "uint32 C=A+3*B; logical bytes, not physical DRAM traffic; one dispatch per sample. "
              "All output words checked outside GPU/host encoding timing.";
  const std::array inputs{std::as_bytes(std::span(a)), std::as_bytes(std::span(b))};
  return Compute(device, pipeline, inputs, std::as_bytes(std::span(expected)), std::move(work));
}
}

Native::Result<std::vector<Workload>> GpuWorkloads(Native::Device &device, Shaders shaders) {
  if (device.Limits().MaxWorkgroupInvocations < CALIBRATION_LOCAL_SIZE ||
      device.Limits().MaxWorkgroupSize[0] < CALIBRATION_LOCAL_SIZE ||
      device.Limits().MaxStorageBufferBytes < 33554432u) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Unsupported,
                      .Message = "calibration needs 256 lanes and 32 MiB storage buffers"});
  }
  auto fma =
      device.CreateComputePipeline(shaders.Fma, {.Entry = "main", .Label = "calibration-fma"});
  if (!fma) { return std::unexpected(fma.error()); }
  auto triad =
      device.CreateComputePipeline(shaders.Triad, {.Entry = "main", .Label = "calibration-triad"});
  if (!triad) { return std::unexpected(triad.error()); }
  std::vector<Workload> result;
  auto arithmetic = Fma(device, **fma);
  if (!arithmetic) { return std::unexpected(arithmetic.error()); }
  result.push_back(std::move(*arithmetic));
  for (const size_t elements : {size_t{65536}, size_t{8388608}}) {
    auto stream = Triad(device, **triad, elements);
    if (!stream) { return std::unexpected(stream.error()); }
    result.push_back(std::move(*stream));
  }
  auto raster = Raster(device, shaders);
  if (!raster) { return std::unexpected(raster.error()); }
  result.push_back(std::move(*raster));
  return result;
}
}
