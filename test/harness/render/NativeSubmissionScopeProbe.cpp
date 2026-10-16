#include "NativeGpuCheck.h"
#include "Check.h"
#include "src/render/device/native/vulkan/Buffer.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 192;
constexpr uint32_t kHeight = 16;
constexpr uint32_t kChannels = 4;
constexpr uint32_t kBandWidth = 64;
constexpr float kBandLow = .1f;
constexpr float kBandHigh = .8f;
constexpr uint64_t kRowBytes = uint64_t{kWidth} * kChannels * sizeof(float);
constexpr uint64_t kImageBytes = kRowBytes * kHeight;
constexpr uint32_t kWords = 4096;
constexpr uint32_t kMultiplier = 97;
constexpr uint32_t kBias = 17;
constexpr auto kWait = std::chrono::seconds(5);

void Complete(Device &device, Completion done) {
  CHECK(Require(device.Wait(done, kWait)) == CompletionStatus::Ready,
        "the actual noncompute submission completes within the offline wait budget");
}

void Timing(Device &device, Completion done, std::string_view scope) {
  const auto timing = Require(device.SubmissionGpuTime(done));
  CHECK(timing.Status == CompletionStatus::Ready && std::isfinite(timing.ElapsedNanoseconds) &&
            timing.ElapsedNanoseconds >= 0,
        "copy and raster submissions have a real finite GPU interval without dispatches");
  const auto dispatches = Require(device.GpuTimings(done));
  CHECK(dispatches.Status == CompletionStatus::Ready && dispatches.DispatchCount == 0 &&
            dispatches.Dispatches.empty() && !dispatches.Truncated,
        "submission timing is available independently of any dispatch timing samples");
  std::println("{} submission_gpu_ns={}", scope, timing.ElapsedNanoseconds);
}

void CopyOnly() {
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = true}));
  auto input = Require(device->CreateBuffer({.Bytes = uint64_t{kWords} * sizeof(uint32_t),
                                             .Use = BufferUse::CopySource,
                                             .Placement = Memory::Upload}));
  auto mapped = Require(device->MapWrite(*input, {.Bytes = input->Description().Bytes}));
  for (uint32_t at = 0; at < kWords; ++at) {
    const uint32_t value = at * kMultiplier + kBias;
    std::memcpy(mapped.data() + at * sizeof(value), &value, sizeof(value));
  }
  Require(device->Unmap(*input));
  const auto *native = dynamic_cast<const Vulkan::VulkanBuffer *>(input.get());
  CHECK(native != nullptr, "the actual input allocation can be observed independently");
  if (native == nullptr) { Failed(); }
  const std::weak_ptr<Vulkan::BufferState> weak = native->State();
  auto output = Require(device->CreateBuffer({.Bytes = input->Description().Bytes,
                                              .Use = BufferUse::CopyDestination,
                                              .Placement = Memory::Readback}));
  for (uint32_t at = 0; at < 16; ++at) {
    auto cancelled = Require(device->Begin());
    Require(cancelled->Copy(*input, *output, {.Bytes = input->Description().Bytes}));
  }
  auto commands = Require(device->Begin());
  Require(commands->Copy(*input, *output, {.Bytes = input->Description().Bytes}));
  input.reset();
  const auto done = Require(device->Submit(std::move(commands)));
  CHECK(!weak.expired(), "accepted measured copies retain input until completion is reaped");
  Complete(*device, done);
  CHECK(weak.expired(), "retained timing metadata does not retain a completed GPU buffer");
  Timing(*device, done, "COPY");
  const auto bytes = Require(device->MapRead(*output, {.Bytes = output->Description().Bytes}));
  for (uint32_t at = 0; at < kWords; ++at) {
    uint32_t actual = 0;
    std::memcpy(&actual, bytes.data() + at * sizeof(actual), sizeof(actual));
    CHECK(actual == at * kMultiplier + kBias,
          "every copied word matches the independent CPU pattern after timed cancellations");
  }
  Require(device->Unmap(*output));
}

std::vector<float>
Raster(std::span<const uint32_t> vertex, std::span<const uint32_t> fragment, bool timed) {
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = timed}));
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  auto pipeline = Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                          .Fragment = ShaderCode{.Spirv = fragment},
                                                          .Streams = {},
                                                          .Attributes = {},
                                                          .Colours = targets,
                                                          .Depth = {},
                                                          .Culling = Cull::None,
                                                          .Label = "submission scope"}));
  auto image = Require(
      device->CreateTexture({.Width = kWidth,
                             .Height = kHeight,
                             .Texels = Format::Rgba32Float,
                             .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto output = Require(device->CreateBuffer(
      {.Bytes = kImageBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array colours{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  auto commands = Require(device->Begin());
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  Require(commands->Draw({.State = {.Pipeline = pipeline.get(),
                                    .Bindings = {},
                                    .Vertices = {},
                                    .Area = {.Width = kWidth, .Height = kHeight},
                                    .Clip = {.Width = kWidth, .Height = kHeight}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *image, *output, {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  image.reset();
  pipeline.reset();
  const auto done = Require(device->Submit(std::move(commands)));
  Complete(*device, done);
  if (timed) { Timing(*device, done, "RASTER"); }
  const auto bytes = Require(device->MapRead(*output, {.Bytes = kImageBytes}));
  std::vector<float> pixels(kImageBytes / sizeof(float));
  std::memcpy(pixels.data(), bytes.data(), kImageBytes);
  Require(device->Unmap(*output));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const uint32_t band = x / kBandWidth;
      std::array<float, 4> expected{kBandLow, kBandLow, kBandLow, 1.0f};
      expected[band] = kBandHigh;
      for (uint32_t channel = 0; channel < kChannels; ++channel) {
        CHECK(pixels[(y * kWidth + x) * kChannels + channel] == expected[channel],
              "every timed and untimed raster component matches the independent band formula");
      }
    }
  }
  return pixels;
}

void Write(std::span<const float> pixels, const char *path) {
  std::ofstream file(path, std::ios::binary);
  file << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
  for (size_t at = 0; at < pixels.size(); at += kChannels) {
    const std::array<char, 3> rgb{static_cast<char>(std::lround(pixels[at] * 255)),
                                  static_cast<char>(std::lround(pixels[at + 1] * 255)),
                                  static_cast<char>(std::lround(pixels[at + 2] * 255))};
    file.write(rgb.data(), rgb.size());
  }
  file.flush();
  CHECK(file, "the diagnostic PPM stores the actual measured GPU pixels");
}

}

int main(int argc, const char *const *argv) {
  if (argc != 4) { return 2; }
  CopyOnly();
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  const auto untimed = Raster(vertex, fragment, false);
  const auto timed = Raster(vertex, fragment, true);
  CHECK(timed == untimed, "enabling full-submission timestamps preserves every raster byte");
  Write(timed, argv[3]);
  return Report();
}
