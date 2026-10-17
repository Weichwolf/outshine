#include "NativeGpuCheck.h"
#include "Check.h"
#include "src/render/device/ReadbackTransfer.h"
#ifdef __linux__
#include "src/render/device/native/vulkan/VulkanGraphics.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using outshine::Test::Gpu::Require;
using outshine::Test::Gpu::Spirv;
constexpr uint32_t kSide = 64;
constexpr size_t kPixels = size_t{kSide} * kSide;
constexpr double kCorner = 0.75;
constexpr double kEdgeTolerance = 1e-5;
constexpr double kDepthTolerance = 2e-6;
constexpr float kBelow = -0.25f;
constexpr float kLowerInside = 0.25f;
constexpr float kUpperInside = 0.75f;
constexpr float kAbove = 1.25f;
constexpr uint8_t kLitChannel = 255;
constexpr float kClearDepth = 0.5f;
constexpr auto kWait = std::chrono::seconds(5);

struct alignas(16) Parameters {
  std::array<float, 4> Values{};
};

static_assert(sizeof(Parameters) == 16 && alignof(Parameters) == 16 &&
              offsetof(Parameters, Values) == 0);

struct Images {
  std::unique_ptr<Texture> Colour;
  std::unique_ptr<Texture> Depth;
};

struct Pixels {
  std::vector<uint8_t> Colour;
  std::vector<float> Depth;
};

struct Case {
  std::array<float, 3> Depths;
  bool Reverse = false;
  bool Clamp = false;
  std::string Name;
};

Images Targets(Device &device) {
  return {.Colour = Require(
              device.CreateTexture({.Width = kSide,
                                    .Height = kSide,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource})),
          .Depth = Require(
              device.CreateTexture({.Width = kSide,
                                    .Height = kSide,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}))};
}

Parameters Uniforms(const Case &variant) {
  Parameters result;
  for (size_t i = 0; i < variant.Depths.size(); ++i) {
    result.Values[i] = variant.Reverse ? 1 - variant.Depths[i] : variant.Depths[i];
  }
  result.Values[3] = variant.Reverse ? 3 : 1;
  return result;
}

Pixels Read(Device &device, const Images &images) {
  const auto fetch = [&](const Texture &image) {
    auto transfer =
        Require(ReadbackTransfer::Start(device, image, {.Width = kSide, .Height = kSide}));
    const auto bytes = Require(transfer.Wait(device, kWait));
    CHECK(bytes && bytes->size() == kPixels * sizeof(uint32_t),
          "the complete tightly packed colour or depth image is readable");
    if (!bytes || bytes->size() != kPixels * sizeof(uint32_t)) { outshine::Test::Gpu::Failed(); }
    return std::vector<std::byte>(bytes->begin(), bytes->end());
  };
  const auto colour = fetch(*images.Colour);
  const auto depth = fetch(*images.Depth);
  Pixels result{.Colour = std::vector<uint8_t>(colour.size()),
                .Depth = std::vector<float>(kPixels)};
  std::memcpy(result.Colour.data(), colour.data(), colour.size());
  std::memcpy(result.Depth.data(), depth.data(), depth.size());
  return result;
}

Pixels Draw(Device &device, GraphicsPipeline &pipeline, const Case &variant) {
  auto images = Targets(device);
  auto commands = Require(device.Begin());
  const std::array colours{
      ColourAttachment{.Image = {.Image = images.Colour.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass(
      {.Colours = colours,
       .Depth = DepthAttachment{
           .Image = {.Image = images.Depth.get()}, .Begin = Load::Clear, .Clear = kClearDepth}}));
  const auto data = Uniforms(variant);
  const std::array bindings{
      Binding{.Set = 1, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&data, 1))}}};
  Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = kSide, .Height = kSide},
                                    .Clip = {.Width = kSide, .Height = kSide}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  CHECK(Require(device.Wait(Require(device.Submit(std::move(commands))), kWait)) ==
            CompletionStatus::Ready,
        "the actual raster work completes before independent readback");
  return Read(device, images);
}

struct Expectation {
  double Depth = kClearDepth;
  bool Covered = false;
};

std::optional<Expectation> ExpectedAt(const Case &variant, size_t at) {
  const size_t x = at % kSide;
  const size_t y = at / kSide;
  const double nx = 2 * (static_cast<double>(x) + 0.5) / kSide - 1;
  const double ny = 1 - 2 * (static_cast<double>(y) + 0.5) / kSide;
  const double u = (nx + kCorner) / (2 * kCorner);
  const double v = (ny + kCorner) / (2 * kCorner);
  const double a = 1 - u - v;
  if (std::min({std::abs(u), std::abs(v), std::abs(a)}) < kEdgeTolerance) { return {}; }
  double z = a * variant.Depths[0] + u * variant.Depths[1] + v * variant.Depths[2];
  if (variant.Reverse) { z = 1 - z; }
  if (std::abs(z) < kEdgeTolerance || std::abs(z - 1) < kEdgeTolerance) { return {}; }
  const bool covered = u > 0 && v > 0 && a > 0 && (variant.Clamp || (z >= 0 && z <= 1));
  return Expectation{.Depth = covered ? std::clamp(z, 0.0, 1.0) : kClearDepth, .Covered = covered};
}

void Verify(const Pixels &pixels, const Case &variant) {
  size_t colourErrors = 0;
  size_t depthErrors = 0;
  size_t examined = 0;
  size_t occupied = 0;
  double maximumDepthError = 0;
  for (size_t at = 0; at < kPixels; ++at) {
    const auto expected = ExpectedAt(variant, at);
    if (!expected) { continue; }
    const uint8_t lit = expected->Covered ? kLitChannel : 0;
    const std::array<uint8_t, 4> colour{lit, 0, 0, lit};
    for (size_t c = 0; c < colour.size(); ++c) {
      if (pixels.Colour[at * colour.size() + c] != colour[c]) { ++colourErrors; }
    }
    const double error = std::abs(pixels.Depth[at] - expected->Depth);
    if (!std::isfinite(pixels.Depth[at]) || error > kDepthTolerance) { ++depthErrors; }
    maximumDepthError = std::max(maximumDepthError, error);
    occupied += expected->Covered ? 1 : 0;
    ++examined;
  }
  std::println(
      "CASE {} examined={} occupied={} colour_errors={} depth_errors={} max_depth_error={}",
      variant.Name,
      examined,
      occupied,
      colourErrors,
      depthErrors,
      maximumDepthError);
  CHECK(examined > kPixels / 2, "the independent expectation covers the image, not only a sample");
  CHECK(colourErrors == 0, "clipping and clamping preserve the independently derived coverage");
  CHECK(depthErrors == 0, "fragment depths obey the range after interpolation, not vertex clamp");
}

void Save(const Pixels &pixels, const Case &variant, const char *directory) {
  if (directory == nullptr) { return; }
  const auto store = [&](std::string_view extension, std::span<const std::byte> bytes) {
    const auto path = std::filesystem::path(directory) / (variant.Name + std::string(extension));
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    CHECK(file.good(), "explicit diagnostic output is saved completely");
  };
  store(".rgba8", std::as_bytes(std::span(pixels.Colour)));
  store(".depth32f", std::as_bytes(std::span(pixels.Depth)));
}

void Unsupported() {
#ifdef __linux__
  const Vulkan::Context disabled;
  const GraphicsDescription depthOnly{.Vertex = {},
                                      .Fragment = {},
                                      .Streams = {},
                                      .Attributes = {},
                                      .Colours = {},
                                      .Depth = DepthTarget{},
                                      .Label = {}};
  CHECK(!disabled.Limits.DepthClamp && !depthOnly.ClampDepth,
        "unsupported capability and default clip mode are distinct false defaults");
  CHECK(Vulkan::CheckGraphics(disabled, depthOnly), "default clip needs no depth-clamp feature");
  auto requested = depthOnly;
  requested.ClampDepth = true;
  const auto rejected = Vulkan::CheckGraphics(disabled, requested);
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Unsupported,
        "the actual Vulkan validation refuses clamp when its device feature is disabled");
#endif
}

void Cases(Device &device,
           std::span<const uint32_t> vertex,
           std::span<const uint32_t> fragment,
           const char *directory) {
  const std::array colours{ColourTarget{.Rgb = {}, .Alpha = {}}};
  const std::array<std::array<float, 3>, 5> depths{{{kBelow, kBelow, kBelow},
                                                    {kLowerInside, kLowerInside, kLowerInside},
                                                    {kUpperInside, kUpperInside, kUpperInside},
                                                    {kAbove, kAbove, kAbove},
                                                    {kBelow, kClearDepth, kAbove}}};
  for (const bool clamp : {false, true}) {
    const GraphicsDescription description{.Vertex = {.Spirv = vertex},
                                          .Fragment = ShaderCode{.Spirv = fragment},
                                          .Streams = {},
                                          .Attributes = {},
                                          .Colours = colours,
                                          .Depth = DepthTarget{.Test = false},
                                          .Culling = Cull::None,
                                          .ClampDepth = clamp,
                                          .Label = {}};
    auto made = device.CreateGraphicsPipeline(description);
    if (clamp && !device.Limits().DepthClamp) {
      CHECK(!made && made.error().Kind == ErrorKind::Unsupported,
            "the public device honestly rejects an unavailable clamp mode");
      continue;
    }
    auto pipeline = Require(std::move(made));
    for (const bool reverse : {false, true}) {
      for (size_t i = 0; i < depths.size(); ++i) {
        const Case variant{.Depths = depths[i],
                           .Reverse = reverse,
                           .Clamp = clamp,
                           .Name = std::string(clamp ? "clamp" : "clip") +
                                   (reverse ? "-reverse-" : "-normal-") + std::to_string(i)};
        const auto pixels = Draw(device, *pipeline, variant);
        Verify(pixels, variant);
        Save(pixels, variant, directory);
      }
    }
  }
}
}

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) { return 1; }
  auto device = Require(CreateDevice({.Validation = true}));
  Unsupported();
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  if (argc == 4) {
    std::error_code failed;
    std::filesystem::create_directories(argv[3], failed);
    CHECK(!failed, "the explicit diagnostic directory opens");
    if (failed) { return Report(); }
  }
  Cases(*device, vertex, fragment, argc == 4 ? argv[3] : nullptr);
  Covers("normal and reverse projection at both depth boundaries, constant and sloped planes, "
         "nonunit homogeneous W, depth writes and coverage; unavailable clamp is explicit");
  return Report();
}
