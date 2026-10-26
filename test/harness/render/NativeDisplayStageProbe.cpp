#include "src/render/stages/TonemapStage.h"
#include "src/render/stages/StageSubmission.h"
#include "src/render/device/ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "NativeSdr.h"
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
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 7;
constexpr uint32_t kHeight = 5;
constexpr size_t kPixels = size_t{kWidth} * kHeight;
constexpr float kCoverage = 3.0f / 8.0f;
constexpr float kPoisonedHistory = 1000.0f;
using Hdr = std::array<float, 4 * kPixels>;

Hdr Input() {
  Hdr pixels{};
  for (size_t i = 0; i < kPixels; ++i) {
    pixels[4 * i] = static_cast<float>(i % 8) / 8;
    pixels[4 * i + 1] = 1 - pixels[4 * i];
    pixels[4 * i + 2] = 3;
    pixels[4 * i + 3] = kCoverage;
  }
  return pixels;
}

std::unique_ptr<Texture>
Upload(Device &device, Format format, std::span<const std::byte> data, uint64_t rowBytes) {
  auto texture =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = format,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto upload = Require(device.CreateBuffer(
      {.Bytes = data.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto bytes = Require(device.MapWrite(*upload, {.Bytes = data.size()}));
  std::memcpy(bytes.data(), data.data(), data.size());
  Require(device.Unmap(*upload));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *upload, *texture, {.RowBytes = rowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "offline input upload completes before the next bounded submission");
  return texture;
}

void CheckPixels(
    Device &device, Texture &image, const Hdr &input, bool temporal, std::string_view output) {
  auto transfer =
      Require(ReadbackTransfer::Start(device, image, {.Width = kWidth, .Height = kHeight}));
  const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "the actual stage output is available within the explicit offline read budget");
  if (!bytes) { Failed(); }
  std::array<uint8_t, 4 * kPixels> pixels{};
  std::memcpy(pixels.data(), bytes->data(), sizeof(pixels));
  for (size_t i = 0; i < kPixels; ++i) {
    for (size_t c = 0; c < 3; ++c) {
      const int delta = static_cast<int>(pixels[4 * i + c]) -
                        SdrByte(std::clamp(input[4 * i + c], 0.0f, 1.0f), Format::Rgba8Srgb);
      CHECK(std::abs(delta) <= 1,
            "unit linear exposure preserves scene light before one SDR encode");
    }
    CHECK(pixels[4 * i + 3] == (temporal ? 96 : 255),
          "the ordinary and temporal alpha contracts retain their existing behaviour");
  }
  if (!output.empty()) {
    std::ofstream file(std::string(output) + (temporal ? "-temporal.ppm" : "-tonemap.ppm"),
                       std::ios::binary);
    file << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (size_t i = 0; i < kPixels; ++i) {
      file.write(reinterpret_cast<const char *>(pixels.data() + 4 * i), 3);
    }
    file.flush();
    CHECK(file, "the optional diagnostic image contains actual stage readback pixels");
  }
}

void Render(Device &device, const Hdr &input, bool temporal, std::string_view output) {
  auto scene = Upload(device,
                      Format::Rgba32Float,
                      std::as_bytes(std::span(input)),
                      uint64_t{kWidth} * 4 * sizeof(float));
  std::array<float, kPixels> depthValues{};
  depthValues.fill(0.5f);
  auto depth = Upload(
      device, Format::Depth32Float, std::as_bytes(std::span(depthValues)), kWidth * sizeof(float));
  Hdr historyValues{};
  historyValues.fill(kPoisonedHistory);
  auto history = Upload(device,
                        Format::Rgba32Float,
                        std::as_bytes(std::span(historyValues)),
                        uint64_t{kWidth} * 4 * sizeof(float));
  const Hdr zeros{};
  auto velocity = Upload(device,
                         Format::Rgba32Float,
                         std::as_bytes(std::span(zeros)),
                         uint64_t{kWidth} * 4 * sizeof(float));
  auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest, .MaxLod = 0}));
  auto display =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba8Srgb,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto linear =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  const outshine::Render::Gpu gpu{.Device = &device,
                                  .HdrFormat = Format::Rgba32Float,
                                  .SurfaceFormat = Format::Rgba8Srgb,
                                  .Width = kWidth,
                                  .Height = kHeight,
                                  .SceneColours = {}};
  TonemapStage stage;
  Require(stage.Configure(gpu,
                          {.Scene = scene.get(),
                           .Depth = depth.get(),
                           .Exact = sampler.get(),
                           .Linear = Format::Rgba32Float},
                          {.Exposure = 1, .Curve = Transfer::Linear, .Temporal = temporal}));
  stage.BindTemporal({.History = history.get(), .Velocity = velocity.get()},
                     {.WidthPx = kWidth, .HeightPx = kHeight},
                     {},
                     false);
  auto commands = Require(device.Begin());
  const std::array colours{
      ColourAttachment{.Image = {.Image = temporal ? linear.get() : display.get()},
                       .Begin = Load::Clear},
      ColourAttachment{.Image = {.Image = display.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass(
      {.Colours = std::span(colours.data(), temporal ? 2u : 1u), .Depth = {}}));
  StageSubmission submission;
  Require(stage.Encode({},
                       {.Commands = commands.get(),
                        .Submission = submission,
                        .Area = {.Width = kWidth, .Height = kHeight},
                        .Clip = {.Width = kWidth, .Height = kHeight}}));
  Require(commands->EndRenderPass());
  Require(device.Submit(std::move(commands)));
  CheckPixels(device, *display, input, temporal, output);
  if (temporal) {
    auto transfer =
        Require(ReadbackTransfer::Start(device, *linear, {.Width = kWidth, .Height = kHeight}));
    const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
    CHECK(bytes && std::ranges::equal(*bytes, std::as_bytes(std::span(input))),
          "a fresh temporal frame preserves every linear MRT byte despite poisoned history");
  }
}
}

int main(int argc, char **argv) {
  CHECK(argc <= 2, "the stage probe accepts only an optional diagnostic output prefix");
  if (argc > 2) { return Report(); }
  const std::string_view output = argc == 2 ? argv[1] : "";
  auto device = Require(CreateDevice({.Validation = true}));
  const auto input = Input();
  Render(*device, input, false, output);
  Render(*device, input, true, output);
  return Report();
}
