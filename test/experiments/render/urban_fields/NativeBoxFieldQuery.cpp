#include "NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kMagic = 0x32425155;
constexpr size_t kWordBytes = sizeof(uint32_t);
constexpr size_t kMaxInputBytes = size_t{64} * 1024 * 1024;
constexpr uint32_t kMaxSide = 16384;
constexpr uint32_t kMaxRays = 2097152;
constexpr uint32_t kPaletteWords = 256;
constexpr uint32_t kRayWords = 8;
constexpr uint32_t kCameraWords = 16;
constexpr uint32_t kWarmFrames = 3;
constexpr uint32_t kMeasuredFrames = 60;
constexpr size_t kPrefixWords = 4;
constexpr size_t kParameterBytes = 304;
constexpr size_t kLevelOffsetBytes = 48;
constexpr double kNanosecondsPerMillisecond = 1e6;

struct alignas(16) Parameters {
  std::array<uint32_t, 4> Shape{};
  std::array<float, 4> Metric{};
  std::array<uint32_t, 4> View{};
  std::array<std::array<uint32_t, 4>, 16> Levels{};
};

struct alignas(16) Hit {
  float Distance = 0;
  uint32_t Face = 0, Evaluations = 0, Flags = 0;
};

static_assert(sizeof(Parameters) == kParameterBytes && alignof(Parameters) == 16 &&
              offsetof(Parameters, Shape) == 0 && offsetof(Parameters, Metric) == 16 &&
              offsetof(Parameters, View) == 32 &&
              offsetof(Parameters, Levels) == kLevelOffsetBytes);
static_assert(sizeof(Hit) == 16 && alignof(Hit) == 16 && offsetof(Hit, Distance) == 0 &&
              offsetof(Hit, Face) == 4 && offsetof(Hit, Evaluations) == 8 &&
              offsetof(Hit, Flags) == 12);
constexpr size_t kHeaderWords = kPrefixWords + sizeof(Parameters) / kWordBytes;

std::vector<uint32_t> Input(const char *name) {
  std::ifstream file(name, std::ios::binary | std::ios::ate);
  const std::streamoff size = file ? static_cast<std::streamoff>(file.tellg()) : -1;
  const bool fits = std::cmp_greater_equal(size, kHeaderWords * kWordBytes) &&
                    std::cmp_less_equal(size, kMaxInputBytes) &&
                    size % static_cast<std::streamoff>(kWordBytes) == 0;
  CHECK(fits, "query input has a bounded word-aligned header");
  if (!fits) { Failed(); }
  std::vector<uint32_t> words(static_cast<size_t>(size) / kWordBytes);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), size);
  CHECK(file.good(), "complete query input is readable");
  if (!file.good()) { Failed(); }
  return words;
}

bool Valid(const Parameters &p, std::span<const uint32_t> words) {
  const size_t fields = words[0];
  const size_t rays = words[1];
  const auto &s = p.Shape;
  const bool view = p.View[2] == 0 ? rays == size_t{kRayWords} * s[3]
                                   : p.View[2] == 1 && rays == kCameraWords && p.View[0] > 0 &&
                                         p.View[1] > 0 && uint64_t{p.View[0]} * p.View[1] == s[3];
  if (words[2] != kMagic || words[3] != 0 || p.View[3] != 0 || s[0] == 0 || s[1] == 0 ||
      s[0] > kMaxSide || s[1] > kMaxSide || s[2] >= p.Levels.size() || s[3] == 0 ||
      s[3] > kMaxRays || !view || fields < kPaletteWords || fields > kMaxRays ||
      words.size() != kHeaderWords + fields + rays ||
      !std::ranges::all_of(p.Metric, [](float value) { return std::isfinite(value); }) ||
      p.Metric[0] <= 0 || p.Metric[1] <= 0 || p.Metric[1] > 1 || p.Metric[2] < 0 ||
      p.Metric[3] <= 0) {
    return false;
  }
  uint32_t columns = s[0];
  uint32_t rows = s[1];
  for (uint32_t level = 0; level <= s[2]; ++level) {
    const auto &info = p.Levels[level];
    if (info[0] < kPaletteWords * kWordBytes || info[1] != columns + 1 || info[2] != columns ||
        info[3] != rows ||
        uint64_t{info[0]} + uint64_t{info[1]} * (uint64_t{rows} + 1) > fields * kWordBytes) {
      return false;
    }
    if (level == s[2] && (columns != 1 || rows != 1)) { return false; }
    columns = (columns + 1) / 2;
    rows = (rows + 1) / 2;
  }
  return true;
}

void Measure(Device &device,
             const ComputePipeline &pipeline,
             std::span<const Binding> bindings,
             uint32_t rays,
             const std::string &prefix) {
  std::ofstream times(prefix + ".times");
  const auto width = pipeline.Layout().LocalSize[0];
  for (uint32_t frame = 0; frame < kWarmFrames + kMeasuredFrames; ++frame) {
    auto commands = Require(device.Begin());
    Require(commands->Dispatch(pipeline, bindings, {.X = (rays + width - 1) / width}));
    const auto token = Require(device.Submit(std::move(commands)));
    CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
          "query submission completes");
    const auto timing = Require(device.SubmissionGpuTime(token));
    CHECK(timing.Status == CompletionStatus::Ready && std::isfinite(timing.ElapsedNanoseconds),
          "native submission GPU time is available");
    if (frame >= kWarmFrames) {
      times << timing.ElapsedNanoseconds / kNanosecondsPerMillisecond << '\n';
    }
  }
  CHECK(times.good(), "sixty query timing samples are written");
}

void Save(Device &device, const Buffer &output, const std::string &prefix) {
  const auto bytes = output.Description().Bytes;
  auto copy = Require(device.Begin());
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Require(copy->Copy(output, *readback, {.Bytes = bytes}));
  const auto token = Require(device.Submit(std::move(copy)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "result copy completes");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  std::ofstream result(prefix + ".hits", std::ios::binary);
  result.write(reinterpret_cast<const char *>(mapped.data()),
               static_cast<std::streamsize>(mapped.size()));
  Require(device.Unmap(*readback));
  CHECK(result.good(), "query results are written");
}
}

int main(int argc, const char *const *argv) {
  if (argc != 5 || (std::string(argv[4]) != "normal" && std::string(argv[4]) != "validate")) {
    return 2;
  }
  const auto words = Input(argv[2]);
  Parameters params;
  std::memcpy(&params, words.data() + kPrefixWords, sizeof(params));
  const bool valid = Valid(params, words);
  CHECK(valid, "query buffers, levels and counts agree");
  if (!valid) { Failed(); }
  auto device = Require(CreateDevice(
      {.Validation = std::string(argv[4]) == "validate", .GpuTiming = GpuTimingMode::Submission}));
  std::println("DEVICE {}", device->Limits().Name);
  const auto pipeline = Require(device->CreateComputePipeline(Spirv(argv[1])));
  auto setup = Require(device->Begin());
  const auto field = Upload(*device, *setup, std::span(words).subspan(kHeaderWords, words[0]));
  const auto input =
      Upload(*device, *setup, std::span(words).subspan(kHeaderWords + words[0], words[1]));
  const auto ready = Require(device->Submit(std::move(setup)));
  CHECK(Require(device->Wait(ready, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "input uploads complete before measurement");
  const auto output =
      Require(device->CreateBuffer({.Bytes = uint64_t{sizeof(Hit)} * params.Shape[3],
                                    .Use = BufferUse::Storage | BufferUse::CopySource}));
  const std::array bindings{
      Storage(0, 0, *field),
      Storage(0, 1, *input),
      Storage(1, 0, *output),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&params, 1))}}};
  Measure(*device, *pipeline, bindings, params.Shape[3], argv[3]);
  Save(*device, *output, argv[3]);
  return Report();
}
