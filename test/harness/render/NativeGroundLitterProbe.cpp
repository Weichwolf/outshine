#include "native/Commands.h"
#include "native/Compute.h"
#include "native/Device.h"
#include "native/Resources.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace Gpu = outshine::Render::Native;

constexpr std::streamoff kMaximumInputBytes = std::streamoff{64} * 1024 * 1024;
constexpr uint32_t kThreadCount = 64;
constexpr uint32_t kProfileDispatches = 32;
constexpr double kNanosecondsPerMillisecond = 1e6;
constexpr size_t kProbeStride = 32;
constexpr size_t kWhichOffsetBytes = 24, kModeOffsetBytes = 28;
constexpr auto kCaptureTimeout = std::chrono::seconds(30);

struct alignas(16) Probe {
  std::array<float, 4> PointDx;
  std::array<float, 2> Dy;
  int32_t Which;
  uint32_t Mode;
};

struct alignas(16) Sample {
  std::array<float, 4> Wearing, Detail;
};

struct alignas(16) Parameters {
  uint32_t Count, Unmapped;
  std::array<uint32_t, 2> Padding;
};

static_assert(sizeof(Probe) == kProbeStride && alignof(Probe) == 16 && offsetof(Probe, Dy) == 16 &&
              offsetof(Probe, Which) == kWhichOffsetBytes &&
              offsetof(Probe, Mode) == kModeOffsetBytes);
static_assert(sizeof(Sample) == kProbeStride && alignof(Sample) == 16 &&
              offsetof(Sample, Detail) == 16);
static_assert(sizeof(Parameters) == 16 && alignof(Parameters) == 16 &&
              offsetof(Parameters, Unmapped) == 4 && offsetof(Parameters, Padding) == 8);

template <typename Value> bool Checked(const Gpu::Result<Value> &result) {
  if (!result) { std::println(stderr, "native material probe: {}", result.error().Message); }
  return result.has_value();
}

std::vector<std::byte> Read(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() <= 0 || input.tellg() > kMaximumInputBytes) { return {}; }
  std::vector<std::byte> bytes(static_cast<size_t>(input.tellg()));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return input ? bytes : std::vector<std::byte>{};
}

struct Buffers {
  enum Slot : size_t { Upload, Palette, Inputs, Outputs, Download, Count };

  std::array<std::unique_ptr<Gpu::Buffer>, Count> Owned;

  Gpu::Buffer &operator[](Slot slot) { return *Owned[slot]; }
};

bool Allocate(Gpu::Device &device, uint64_t paletteBytes, uint64_t probeBytes, Buffers &buffers) {
  using enum Gpu::BufferUse;
  const std::array<Gpu::BufferDescription, Buffers::Count> descriptions{{
      {.Bytes = paletteBytes + probeBytes, .Use = CopySource, .Placement = Gpu::Memory::Upload},
      {.Bytes = paletteBytes, .Use = CopyDestination | Storage, .Placement = Gpu::Memory::Device},
      {.Bytes = probeBytes, .Use = CopyDestination | Storage, .Placement = Gpu::Memory::Device},
      {.Bytes = probeBytes, .Use = Storage | CopySource, .Placement = Gpu::Memory::Device},
      {.Bytes = probeBytes, .Use = CopyDestination, .Placement = Gpu::Memory::Readback},
  }};
  for (size_t index = 0; index < descriptions.size(); ++index) {
    auto buffer = device.CreateBuffer(descriptions[index], "ground litter probe");
    if (!Checked(buffer)) { return false; }
    buffers.Owned[index] = std::move(*buffer);
  }
  return true;
}

bool Matches(const Gpu::ComputeLayout &layout) {
  const std::array<Gpu::ShaderBinding, 4> required{{
      {.Set = 0,
       .Slot = 0,
       .Kind = Gpu::BindingKind::StorageBuffer,
       .Usage = Gpu::Access::Read,
       .MinimumBytes = 0},
      {.Set = 0,
       .Slot = 1,
       .Kind = Gpu::BindingKind::StorageBuffer,
       .Usage = Gpu::Access::Read,
       .MinimumBytes = 0},
      {.Set = 1,
       .Slot = 0,
       .Kind = Gpu::BindingKind::StorageBuffer,
       .Usage = Gpu::Access::Write,
       .MinimumBytes = 0},
      {.Set = 2,
       .Slot = 0,
       .Kind = Gpu::BindingKind::Uniform,
       .Usage = Gpu::Access::Read,
       .MinimumBytes = sizeof(Parameters)},
  }};
  if (layout.LocalSize != std::array<uint32_t, 3>{kThreadCount, 1, 1} ||
      layout.Bindings.size() != required.size()) {
    return false;
  }
  for (const auto &need : required) {
    bool found = false;
    for (const auto &actual : layout.Bindings) {
      if (actual.Set == need.Set && actual.Slot == need.Slot && actual.Kind == need.Kind &&
          actual.Usage == need.Usage &&
          (need.Kind != Gpu::BindingKind::Uniform || actual.MinimumBytes == need.MinimumBytes)) {
        found = true;
      }
    }
    if (!found) { return false; }
  }
  return true;
}

void Profile(Gpu::Device &device, Gpu::Completion completion) {
  const auto batch = device.SubmissionGpuTime(completion);
  if (batch && batch->Status == Gpu::CompletionStatus::Ready &&
      std::isfinite(batch->ElapsedNanoseconds) && batch->ElapsedNanoseconds >= 0) {
    std::println("profile_batch_available=1 profile_batch_gpu_ms={:.9g} "
                 "profile_batch_dispatches={} profile_batch_scope=uploads_dispatches_readback",
                 batch->ElapsedNanoseconds / kNanosecondsPerMillisecond,
                 kProfileDispatches);
  } else {
    std::println("profile_batch_available=0");
  }
  const auto result = device.GpuTimings(completion);
  if (!result || result->Status != Gpu::CompletionStatus::Ready || result->Truncated ||
      result->DispatchCount != kProfileDispatches ||
      result->Dispatches.size() != kProfileDispatches) {
    std::println("profile_available=0");
    return;
  }
  double sumNs = 0;
  for (const auto &sample : result->Dispatches) {
    if (!sample.Available || sample.ValidBits == 0 || sample.ValidBits > 64 ||
        !std::isfinite(sample.NanosecondsPerTick) || sample.NanosecondsPerTick <= 0) {
      std::println("profile_available=0");
      return;
    }
    const uint64_t mask = sample.ValidBits == 64 ? std::numeric_limits<uint64_t>::max()
                                                 : (uint64_t{1} << sample.ValidBits) - 1;
    sumNs += static_cast<double>((sample.End - sample.Begin) & mask) * sample.NanosecondsPerTick;
  }
  std::println("profile_available=1 profile_dispatches={} profile_sum_ms={:.9g}",
               result->DispatchCount,
               sumNs / kNanosecondsPerMillisecond);
}

bool Capture(Gpu::Device &device,
             const Gpu::ComputePipeline &pipeline,
             std::span<const std::byte> palette,
             std::span<const std::byte> probes,
             Parameters parameters,
             const char *path,
             bool timed) {
  Buffers buffers;
  if (!Allocate(device, palette.size(), probes.size(), buffers)) { return false; }
  auto upload = device.MapWrite(buffers[Buffers::Upload],
                                {.Offset = 0, .Bytes = palette.size() + probes.size()});
  if (!Checked(upload)) { return false; }
  std::memcpy(upload->data(), palette.data(), palette.size());
  std::memcpy(upload->data() + palette.size(), probes.data(), probes.size());
  if (!Checked(device.Unmap(buffers[Buffers::Upload]))) { return false; }
  auto commands = device.Begin();
  if (!Checked(commands)) { return false; }
  if (!Checked((*commands)->Copy(
          buffers[Buffers::Upload],
          buffers[Buffers::Palette],
          {.SourceOffset = 0, .DestinationOffset = 0, .Bytes = palette.size()})) ||
      !Checked((*commands)->Copy(
          buffers[Buffers::Upload],
          buffers[Buffers::Inputs],
          {.SourceOffset = palette.size(), .DestinationOffset = 0, .Bytes = probes.size()}))) {
    return false;
  }
  const std::array<Gpu::Binding, 4> bindings{{
      {.Set = 0,
       .Slot = 0,
       .Value = Gpu::StorageBuffer{.Data = &buffers[Buffers::Palette],
                                   .Range = {.Offset = 0, .Bytes = palette.size()}}},
      {.Set = 0,
       .Slot = 1,
       .Value = Gpu::StorageBuffer{.Data = &buffers[Buffers::Inputs],
                                   .Range = {.Offset = 0, .Bytes = probes.size()}}},
      {.Set = 1,
       .Slot = 0,
       .Value = Gpu::StorageBuffer{.Data = &buffers[Buffers::Outputs],
                                   .Range = {.Offset = 0, .Bytes = probes.size()}}},
      {.Set = 2,
       .Slot = 0,
       .Value = Gpu::Uniform{std::as_bytes(std::span{&parameters, size_t{1}})}},
  }};
  (*commands)->BeginLabel("ground litter mask and composition");
  bool dispatched = true;
  const uint32_t repeats = timed ? kProfileDispatches : 1;
  for (uint32_t repeat = 0; repeat < repeats && dispatched; ++repeat) {
    dispatched = Checked((*commands)->Dispatch(
        pipeline,
        bindings,
        {.X = (parameters.Count + kThreadCount - 1) / kThreadCount, .Y = 1, .Z = 1}));
  }
  (*commands)->EndLabel();
  if (!dispatched || !Checked((*commands)->Copy(
                         buffers[Buffers::Outputs],
                         buffers[Buffers::Download],
                         {.SourceOffset = 0, .DestinationOffset = 0, .Bytes = probes.size()}))) {
    return false;
  }
  auto completion = device.Submit(std::move(*commands));
  if (!Checked(completion)) { return false; }
  auto waited = device.Wait(*completion, kCaptureTimeout);
  if (!Checked(waited) || *waited != Gpu::CompletionStatus::Ready) { return false; }
  if (timed) { Profile(device, *completion); }
  auto download = device.MapRead(buffers[Buffers::Download], {.Offset = 0, .Bytes = probes.size()});
  if (!Checked(download)) { return false; }
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(download->data()),
               static_cast<std::streamsize>(download->size()));
  output.close();
  const bool unmapped = Checked(device.Unmap(buffers[Buffers::Download]));
  std::println("device={} probes={} buffer_bytes={} copy_bytes={} gpu_ms={}",
               device.Limits().Name,
               parameters.Count,
               2 * palette.size() + 4 * probes.size(),
               palette.size() + 2 * probes.size(),
               timed ? "see_profile" : "unmeasured");
  return unmapped && static_cast<bool>(output);
}

bool FitsPalette(std::span<const std::byte> palette, uint32_t rows) {
  const uint64_t floatCount = palette.size() / sizeof(float);
  const uint64_t prefix = 4 * (uint64_t{rows} + 2) + rows + 1 + 8 * (uint64_t{rows} + 1);
  if (rows == 0 || prefix > floatCount) { return false; }
  uint32_t layersAt = 0;
  std::memcpy(&layersAt, palette.data() + 3 * sizeof(float), sizeof(layersAt));
  return layersAt == 0 || (layersAt >= prefix && layersAt <= floatCount &&
                           8 * (uint64_t{rows} + 1) <= floatCount - layersAt);
}

bool Run(const char *const *argv, bool timed) {
  const auto code = Read(argv[1]);
  const auto palette = Read(argv[2]);
  const auto probes = Read(argv[3]);
  if (code.empty() || code.size() % sizeof(uint32_t) != 0 || palette.size() < sizeof(Parameters) ||
      palette.size() % sizeof(float) != 0 || probes.empty() || probes.size() % kProbeStride != 0) {
    return false;
  }
  uint32_t rows = 0;
  uint32_t unmapped = 0;
  std::memcpy(&rows, palette.data(), sizeof(rows));
  const std::string argument(argv[5]);
  const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), unmapped);
  if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size() ||
      unmapped >= rows || !FitsPalette(palette, rows)) {
    return false;
  }
  std::vector<uint32_t> words(code.size() / sizeof(uint32_t));
  std::memcpy(words.data(), code.data(), code.size());
  auto device = Gpu::CreateDevice({.Validation = true, .GpuTiming = timed});
  if (!Checked(device)) { return false; }
  auto pipeline =
      (*device)->CreateComputePipeline(words, {.Entry = "main", .Label = "ground litter probe"});
  if (!Checked(pipeline) || !Matches((*pipeline)->Layout())) { return false; }
  const Parameters parameters{.Count = static_cast<uint32_t>(probes.size() / kProbeStride),
                              .Unmapped = unmapped,
                              .Padding = {0, 0}};
  return Capture(**device, **pipeline, palette, probes, parameters, argv[4], timed);
}
}

int main(int argc, char *const *argv) {
  const bool timed = argc == 7 && std::string_view(argv[6]) == "--gpu-timing";
  if (argc != 6 && !timed) {
    std::fputs("usage: native_ground_litter shader.spv palette.f32 probes.bin result.f32 unmapped "
               "[--gpu-timing]\n",
               stderr);
    return 2;
  }
  const bool ready = Run(argv, timed);
  if (!ready) { std::println(stderr, "native ground litter capture failed"); }
  return ready ? 0 : 1;
}
