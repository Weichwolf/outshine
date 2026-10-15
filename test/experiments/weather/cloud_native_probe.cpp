#include "CloudLayerUniform.h"
#include "native/Device.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <memory>
#include <numbers>
#include <print>
#include <ratio>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
namespace Native = outshine::Render::Native;

struct alignas(16) Pushed {
  Render::CloudLayerUniform Layer;
  Vec4f Origin, Forward, Right, Up, Extent;
};

constexpr size_t kStd140Vec4Bytes = 16;
static_assert(sizeof(Vec4f) == kStd140Vec4Bytes);
static_assert(sizeof(Pushed) == 8 * kStd140Vec4Bytes && alignof(Pushed) == 16);
static_assert(offsetof(Pushed, Origin) == 3 * kStd140Vec4Bytes);
static_assert(offsetof(Pushed, Forward) == 4 * kStd140Vec4Bytes);
static_assert(offsetof(Pushed, Right) == 5 * kStd140Vec4Bytes);
static_assert(offsetof(Pushed, Up) == 6 * kStd140Vec4Bytes);
static_assert(offsetof(Pushed, Extent) == 7 * kStd140Vec4Bytes);

constexpr auto kWorkgroup = std::array{8u, 8u, 1u};
constexpr std::streamoff kMaximumShaderBytes = std::streamoff{64} * 1024 * 1024;
constexpr float kEyeHeightM = 1772.0f;

struct ProbeDimensions {
  int Width = 0;
  int Height = 0;
};

struct Resources {
  std::unique_ptr<Native::Device> Device;
  std::unique_ptr<Native::ComputePipeline> Pipeline;
  std::unique_ptr<Native::Buffer> Results;
  std::unique_ptr<Native::Buffer> Download;
};

template <typename Value> bool Number(std::string_view text, Value &result) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool Failed(std::string_view operation, const Native::Error &error) {
  std::println(stderr, "{}: {}", operation, error.Message);
  return false;
}

std::vector<uint32_t> ReadCode(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = input.tellg();
  if (!input || bytes <= 0 || bytes > kMaximumShaderBytes ||
      bytes % static_cast<std::streamoff>(sizeof(uint32_t)) != 0) {
    return {};
  }
  std::vector<uint32_t> code(static_cast<size_t>(bytes) / sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(code.data()), bytes);
  return input ? std::move(code) : std::vector<uint32_t>{};
}

bool LayoutMatches(const Native::ComputeLayout &layout, uint64_t bytes) {
  if (layout.LocalSize != kWorkgroup || layout.Bindings.size() != 2) { return false; }
  const auto output = std::ranges::find_if(layout.Bindings, [](const Native::ShaderBinding &one) {
    return one.Set == 1 && one.Slot == 0 && one.Kind == Native::BindingKind::StorageBuffer &&
           one.Usage == Native::Access::Write;
  });
  const auto uniform = std::ranges::find_if(layout.Bindings, [](const Native::ShaderBinding &one) {
    return one.Set == 2 && one.Slot == 0 && one.Kind == Native::BindingKind::Uniform &&
           one.Usage == Native::Access::Read;
  });
  return output != layout.Bindings.end() && output->MinimumBytes <= bytes &&
         uniform != layout.Bindings.end() && uniform->MinimumBytes == sizeof(Pushed);
}

bool Configure(Resources &gpu, const char *shaderPath, uint64_t bytes) {
  const auto code = ReadCode(shaderPath);
  if (code.empty()) {
    std::println(stderr, "invalid or unreadable SPIR-V: {}", shaderPath);
    return false;
  }
  auto device = Native::CreateDevice({.Validation = true});
  if (!device) { return Failed("create device", device.error()); }
  gpu.Device = std::move(*device);
  auto pipeline =
      gpu.Device->CreateComputePipeline(code, {.Entry = "main", .Label = "cloud-probe"});
  if (!pipeline) { return Failed("create cloud pipeline", pipeline.error()); }
  if (!LayoutMatches((*pipeline)->Layout(), bytes)) {
    std::fputs("cloud pipeline does not match the two-binding/128-byte/8x8 contract\n", stderr);
    return false;
  }
  gpu.Pipeline = std::move(*pipeline);
  auto results =
      gpu.Device->CreateBuffer({.Bytes = bytes,
                                .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource,
                                .Placement = Native::Memory::Device},
                               "cloud-results");
  if (!results) { return Failed("create cloud results", results.error()); }
  gpu.Results = std::move(*results);
  auto download = gpu.Device->CreateBuffer({.Bytes = bytes,
                                            .Use = Native::BufferUse::CopyDestination,
                                            .Placement = Native::Memory::Readback},
                                           "cloud-readback");
  if (!download) { return Failed("create cloud readback", download.error()); }
  gpu.Download = std::move(*download);
  return true;
}

bool SaveReadback(Resources &gpu, uint64_t bytes, const char *path) {
  const auto mapped = gpu.Device->MapRead(*gpu.Download, {.Offset = 0, .Bytes = bytes});
  if (!mapped) { return Failed("map completed cloud readback", mapped.error()); }
  bool saved = false;
  if (mapped->size() == bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(mapped->data()),
                 static_cast<std::streamsize>(bytes));
    output.close();
    saved = static_cast<bool>(output);
  }
  const auto unmapped = gpu.Device->Unmap(*gpu.Download);
  if (!unmapped) { return Failed("unmap cloud readback", unmapped.error()); }
  if (!saved) { std::println(stderr, "could not save complete cloud readback: {}", path); }
  return saved;
}

bool Capture(Resources &gpu, const Pushed &pushed, uint64_t bytes, const char *path) {
  const auto before = std::chrono::steady_clock::now();
  auto recording = gpu.Device->Begin();
  if (!recording) { return Failed("begin cloud commands", recording.error()); }
  auto commands = std::move(*recording);
  const std::array bindings{
      Native::Binding{.Set = 1,
                      .Slot = 0,
                      .Value = Native::StorageBuffer{.Data = gpu.Results.get(),
                                                     .Range = {.Offset = 0, .Bytes = bytes}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value = Native::Uniform{std::as_bytes(std::span{&pushed, size_t{1}})}}};
  const Native::Workgroups groups{
      .X = (static_cast<uint32_t>(pushed.Extent[0]) + kWorkgroup[0] - 1) / kWorkgroup[0],
      .Y = (static_cast<uint32_t>(pushed.Extent[1]) + kWorkgroup[1] - 1) / kWorkgroup[1],
      .Z = 1};
  commands->BeginLabel("cloud-probe");
  const auto dispatched = commands->Dispatch(*gpu.Pipeline, bindings, groups);
  commands->EndLabel();
  if (!dispatched) { return Failed("dispatch cloud probe", dispatched.error()); }
  const auto copied = commands->Copy(
      *gpu.Results, *gpu.Download, {.SourceOffset = 0, .DestinationOffset = 0, .Bytes = bytes});
  if (!copied) { return Failed("copy cloud readback", copied.error()); }
  const auto encoded = std::chrono::steady_clock::now();
  const auto submission = gpu.Device->Submit(std::move(commands));
  if (!submission) { return Failed("submit cloud probe", submission.error()); }
  const auto submitted = std::chrono::steady_clock::now();
  const auto ready = gpu.Device->Wait(*submission, std::chrono::seconds(30));
  if (!ready) { return Failed("wait for cloud probe", ready.error()); }
  if (*ready != Native::CompletionStatus::Ready) {
    std::fputs("cloud probe did not complete within 30 seconds\n", stderr);
    return false;
  }
  const auto finished = std::chrono::steady_clock::now();
  if (!SaveReadback(gpu, bytes, path)) { return false; }
  std::println("device={} encode_ms={:.6f} submit_ms={:.6f} completion_wait_ms={:.6f} bytes={} "
               "gpu_ms=unmeasured",
               gpu.Device->Limits().Name,
               std::chrono::duration<double, std::milli>(encoded - before).count(),
               std::chrono::duration<double, std::milli>(submitted - encoded).count(),
               std::chrono::duration<double, std::milli>(finished - submitted).count(),
               bytes);
  return true;
}

Pushed Declaration(ProbeDimensions dimensions, double cover) {
  const CloudLayer layer{
      .BaseM = 2572.0, .Cover = cover, .WindMs = CloudWindFrom({.FromDeg = 240.0, .SpeedMs = 8.0})};
  constexpr double bearing = 240.0 * std::numbers::pi / 180.0;
  constexpr double pitch = -0.5 * std::numbers::pi / 180.0;
  const auto halfHeight = static_cast<float>(std::tan(33.97 * std::numbers::pi / 360.0));
  return {.Layer = Render::PackCloudLayer(layer, 0.0),
          .Origin = {{0.0f, kEyeHeightM, 0.0f, 0.0f}},
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
          .Extent = {{static_cast<float>(dimensions.Width),
                      static_cast<float>(dimensions.Height),
                      halfHeight * static_cast<float>(dimensions.Width) /
                          static_cast<float>(dimensions.Height),
                      halfHeight}}};
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
    std::fputs(
        "usage: cloud-native-probe shader.spv output.f32 width height cover [1|4 channels]\n",
        stderr);
    return 2;
  }
  const uint64_t bytes = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) *
                         static_cast<uint64_t>(channels) * sizeof(float);
  Resources gpu;
  if (!Configure(gpu, argv[1], bytes)) { return 1; }
  return Capture(gpu, Declaration({.Width = width, .Height = height}, cover), bytes, argv[2]) ? 0
                                                                                              : 1;
}
