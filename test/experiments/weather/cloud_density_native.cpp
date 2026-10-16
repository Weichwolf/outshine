#include "NativeGpuCheck.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kMaximumPoints = size_t{1024} * 1024;
constexpr size_t kFrameWords = 12;
constexpr size_t kPointWords = 4;
constexpr uint32_t kThreads = 64;
constexpr uint32_t kRepetitions = 16;
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == sizeof(uint32_t));

struct Capture {
  std::vector<uint32_t> Values;
  std::optional<GpuTimingResults> Timing;
};

std::vector<uint32_t> Read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = file ? static_cast<std::streamoff>(file.tellg()) : -1;
  const bool complete =
      std::cmp_greater(bytes, kFrameWords * sizeof(uint32_t)) &&
      std::cmp_less_equal(bytes, (kFrameWords + kPointWords * kMaximumPoints) * sizeof(uint32_t)) &&
      (static_cast<size_t>(bytes) / sizeof(uint32_t) - kFrameWords) % kPointWords == 0 &&
      bytes % sizeof(uint32_t) == 0;
  CHECK(complete, "the bounded input contains one48B frame and complete16B points");
  if (!complete) { Failed(); }
  std::vector<uint32_t> result(static_cast<size_t>(bytes) / sizeof(uint32_t));
  file.seekg(0);
  CHECK(file.read(reinterpret_cast<char *>(result.data()), bytes),
        "all original density inputs are read");
  return result;
}

Capture Run(Device &device, const char *shader, std::span<const uint32_t> data, bool timed) {
  auto pipeline = Require(device.CreateComputePipeline(Spirv(shader)));
  constexpr std::array<uint32_t, 3> localSize{kThreads, 1, 1};
  CHECK(pipeline->Layout().LocalSize == localSize, "the actual workgroup has64 threads");
  size_t uniforms = 0;
  for (const auto &binding : pipeline->Layout().Bindings) {
    if (binding.Set == 2 && binding.Slot == 0 && binding.Kind == BindingKind::Uniform) {
      CHECK(binding.MinimumBytes == kFrameWords * sizeof(uint32_t),
            "the reflected profile frame remains48 bytes");
      ++uniforms;
    }
  }
  CHECK(uniforms == 1, "the private frame is reflected once");
  std::array<uint32_t, kFrameWords> frame;
  std::copy_n(data.begin(), kFrameWords, frame.begin());
  const auto points = data.subspan(kFrameWords);
  const auto count = points.size() / kPointWords;
  const auto bytes = count * sizeof(float);
  auto commands = Require(device.Begin());
  auto input = Upload(device, *commands, points);
  auto output = Require(
      device.CreateBuffer({.Bytes = bytes, .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array bindings{
      Storage(0, 0, *input),
      Storage(1, 0, *output),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(frame))}}};
  for (uint32_t repeat = 0; repeat < kRepetitions; ++repeat) {
    Require(commands->Dispatch(
        *pipeline, bindings, {.X = static_cast<uint32_t>((count + kThreads - 1) / kThreads)}));
  }
  frame.fill(0);
  Require(commands->Copy(*output, *readback, {.Bytes = bytes}));
  input.reset();
  output.reset();
  pipeline.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "the density microprobe completes within its offline wait bound");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  Capture capture{.Values = std::vector<uint32_t>(count), .Timing = std::nullopt};
  std::memcpy(capture.Values.data(), mapped.data(), bytes);
  Require(device.Unmap(*readback));
  if (timed) { capture.Timing = Require(device.GpuTimings(completion)); }
  return capture;
}

void WriteTimes(std::ofstream &file, const Capture &capture) {
  file << '[';
  if (capture.Timing) {
    const auto &timing = *capture.Timing;
    CHECK(timing.Status == CompletionStatus::Ready && !timing.Truncated &&
              timing.DispatchCount == kRepetitions && timing.Dispatches.size() == kRepetitions,
          "completed measurements contain all16 actual dispatch pairs");
    bool first = true;
    for (const auto &sample : timing.Dispatches) {
      const bool valid = sample.Available && sample.ValidBits > 0 && sample.ValidBits <= 64 &&
                         std::isfinite(sample.NanosecondsPerTick) && sample.NanosecondsPerTick > 0;
      CHECK(valid, "each actual GPU pair has valid precision and period");
      if (!valid) { Failed(); }
      const uint64_t mask = std::numeric_limits<uint64_t>::max() >> (64 - sample.ValidBits);
      const double ns =
          static_cast<double>((sample.End - sample.Begin) & mask) * sample.NanosecondsPerTick;
      CHECK(std::isfinite(ns) && ns >= 0, "dispatch duration is finite nonnegative GPU time");
      if (!first) { file << ','; }
      first = false;
      file << R"({"begin":)" << sample.Begin << R"(,"end":)" << sample.End << R"(,"valid_bits":)"
           << sample.ValidBits << R"(,"ns_per_tick":)" << sample.NanosecondsPerTick
           << R"(,"elapsed_ns":)" << ns << '}';
    }
  }
  file << ']';
}
}

int main(int argc, char **argv) {
  const bool timed = argc == 5;
  if (!timed && (argc != 6 || std::string_view(argv[5]) != "untimed")) {
    std::println(
        stderr,
        "usage: cloud-density-native shader.spv input.bin output.bin timings.json [untimed]");
    return 2;
  }
  const auto input = Read(argv[2]);
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = timed}));
  const auto first = Run(*device, argv[1], input, timed);
  const auto repeat = Run(*device, argv[1], input, timed);
  CHECK(first.Values == repeat.Values,
        "independent recorded density outputs are bitwise identical");
  std::ofstream output(argv[3], std::ios::binary);
  CHECK(output.write(reinterpret_cast<const char *>(first.Values.data()),
                     static_cast<std::streamsize>(first.Values.size() * sizeof(uint32_t))),
        "all original native density values are saved");
  std::ofstream times(argv[4]);
  times << std::setprecision(std::numeric_limits<double>::max_digits10) << R"({"device":)"
        << std::quoted(std::string(device->Limits().Name)) << R"(,"points":)" << first.Values.size()
        << R"(,"timed":)" << (timed ? "true" : "false") << R"(,"runs":[)";
  WriteTimes(times, first);
  times << ',';
  WriteTimes(times, repeat);
  times << "]}\n";
  CHECK(times.good(), "raw actual GPU pairs or explicitly untimed state are preserved");
  std::println("points={} repeated_dispatches={}", first.Values.size(), 2 * kRepetitions);
  return Report();
}
