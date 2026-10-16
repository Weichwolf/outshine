#include "CompositeTransmissionStage.h"
#include "AerialPerspectiveStage.h"
#include "SkyStage.h"
#include "StageSubmission.h"
#include "ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "NativeOfflineDevice.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using outshine::kEarthAir;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 7, kHeight = 5;
constexpr size_t kPixels = size_t{kWidth} * kHeight;
constexpr uint64_t kRgbaRowBytes = uint64_t{kWidth} * 4 * sizeof(float);
constexpr uint64_t kScalarRowBytes = uint64_t{kWidth} * sizeof(float);
constexpr float kCoverage = 0.625f;
constexpr float kEighth = 0.125f;
constexpr float kQuarter = 0.25f;
constexpr float kThreeQuarters = 0.75f;
constexpr float kOneAndHalf = 1.5f;
using Pixels = std::array<float, kPixels * 4>;

std::unique_ptr<Texture>
Upload(Device &device, Format format, std::span<const std::byte> bytes, uint64_t row) {
  auto image =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = format,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto staging = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto mapped = Require(device.MapWrite(*staging, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*staging));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *staging, *image, {.RowBytes = row, .Region = {.Width = kWidth, .Height = kHeight}}));
  Require(device.Submit(std::move(commands)));
  return image;
}

std::unique_ptr<Texture> Output(Device &device, Format format, bool depth = false) {
  return Require(device.CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = format,
       .Use = (depth ? TextureUse::DepthAttachment : TextureUse::ColourAttachment) |
              TextureUse::CopySource}));
}

std::vector<std::byte> Read(Device &device, Texture &image) {
  auto read = Require(ReadbackTransfer::Start(device, image, {.Width = kWidth, .Height = kHeight}));
  auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "actual fullscreen output is available within the explicit offline budget");
  if (!bytes) { Failed(); }
  return {bytes->begin(), bytes->end()};
}

void Compare(Device &device, Texture &image, const Pixels &expected) {
  const auto bytes = Read(device, image);
  CHECK(bytes.size() == sizeof(expected), "the real raster output keeps its RGBA32F format");
  CHECK(bytes.size() == sizeof(expected) &&
            std::ranges::equal(bytes, std::as_bytes(std::span(expected))),
        "every actual fullscreen output component matches the independent exact reference");
}

template <class Stage> void Draw(Device &device, Stage &stage, Texture &output) {
  auto commands = Require(device.Begin());
  const std::array attachments{ColourAttachment{.Image = {.Image = &output}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = attachments, .Depth = {}}));
  StageSubmission submission;
  Require(stage.Encode({},
                       {.Commands = commands.get(),
                        .Submission = submission,
                        .Area = {.Width = kWidth, .Height = kHeight},
                        .Clip = {.Width = kWidth, .Height = kHeight}}));
  Require(commands->EndRenderPass());
  Require(device.Submit(std::move(commands)));
  submission.Commit();
}

void Composite(Device &device, Sampler &sampler) {
  Pixels opaque{};
  Pixels front{};
  Pixels expected{};
  for (size_t i = 0; i < kPixels; ++i) {
    opaque[4 * i] = static_cast<float>(i % 8) / 8;
    opaque[4 * i + 1] = 0.5f;
    opaque[4 * i + 2] = 2;
    opaque[4 * i + 3] = kCoverage;
    front[4 * i] = kQuarter;
    front[4 * i + 1] = kEighth;
    front[4 * i + 2] = 0.5f;
    front[4 * i + 3] = static_cast<float>(i % 5) / 4;
    for (size_t c = 0; c < 3; ++c) {
      expected[4 * i + c] = opaque[4 * i + c] * (1 - front[4 * i + 3]) + front[4 * i + c];
    }
    expected[4 * i + 3] = opaque[4 * i + 3];
  }
  auto back = Upload(device, Format::Rgba32Float, std::as_bytes(std::span(opaque)), kRgbaRowBytes);
  auto glass = Upload(device, Format::Rgba32Float, std::as_bytes(std::span(front)), kRgbaRowBytes);
  auto output = Output(device, Format::Rgba32Float);
  CompositeTransmissionStage stage;
  const outshine::Render::Gpu gpu{.Device = &device, .SceneColours = {}};
  Require(stage.Configure(gpu,
                          {.Opaque = back.get(),
                           .Transmissive = glass.get(),
                           .Exact = &sampler,
                           .Target = Format::Rgba32Float}));
  CHECK(!stage.Configure(gpu, {}),
        "incomplete reconfiguration retains the accepted transmission feeds");
  Draw(device, stage, *output);
  Compare(device, *output, expected);
}

void Aerial(Device &device, Sampler &sampler, Texture &sky, Texture &veil) {
  Pixels input{};
  std::array<float, kPixels> depths{};
  for (size_t i = 0; i < kPixels; ++i) {
    input[4 * i] = kQuarter;
    input[4 * i + 1] = 0.5f;
    input[4 * i + 2] = 2;
    input[4 * i + 3] = kThreeQuarters;
    depths[i] = i % 2 == 0 ? 0 : 0.5f;
  }
  auto scene = Upload(device, Format::Rgba32Float, std::as_bytes(std::span(input)), kRgbaRowBytes);
  auto depth = Upload(device, Format::R32Float, std::as_bytes(std::span(depths)), kScalarRowBytes);
  auto output = Output(device, Format::Rgba32Float);
  AerialPerspectiveStage stage;
  Require(stage.Configure({.Device = &device, .SceneColours = {}},
                          {.Scene = scene.get(),
                           .Depth = depth.get(),
                           .SkyView = &sky,
                           .Transmittance = &veil,
                           .Exact = &sampler,
                           .Lut = &sampler},
                          Format::Rgba32Float));
  stage.Declare(kEarthAir, {.SunDir = {{1, 0, 0}}, .WorldUp = {{0, 1, 0}}, .IlluminanceLux = 2});
  stage.SetBasis({.Right = {{1, 0, 0}},
                  .Up = {{0, 1, 0}},
                  .Forward = {{0, 0, 1}},
                  .TanHalfWidth = 0.5f,
                  .TanHalfHeight = 0.5f});
  stage.SetDepthReconstruction({{10, 0, 1, 0}});
  Draw(device, stage, *output);
  Compare(device, *output, input);
  CHECK(!stage.Configure({}, {}, Format::Rgba32Float),
        "an invalid aerial candidate does not overwrite accepted inputs");
  Draw(device, stage, *output);
  Compare(device, *output, input);
}

void Sky(Device &device, Sampler &sampler, Texture &table, Texture &veil) {
  outshine::Render::Gpu gpu{
      .Device = &device, .HdrFormat = Format::Rgba32Float, .SceneColours = {}};
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr) && gpu.SceneColours.Add(Resource::SceneVelocity),
        "the existing sky HDR and static velocity targets are declared");
  auto hdr = Output(device, Format::Rgba32Float);
  auto velocity = Output(device, Format::Rg16Float);
  auto depth = Output(device, Format::Depth32Float, true);
  SkyStage stage;
  Require(stage.Configure(gpu, {.SkyView = &table, .Transmittance = &veil, .Lut = &sampler}));
  stage.Declare(kEarthAir, {.SunDir = {{1, 0, 0}}, .WorldUp = {{0, 1, 0}}, .IlluminanceLux = 2});
  stage.SetBasis({.Right = {{1, 0, 0}},
                  .Up = {{0, 1, 0}},
                  .Forward = {{0, 0, 1}},
                  .TanHalfWidth = 0.5f,
                  .TanHalfHeight = 0.5f});
  auto commands = Require(device.Begin());
  const std::array attachments{
      ColourAttachment{.Image = {.Image = hdr.get()}, .Begin = Load::Clear},
      ColourAttachment{.Image = {.Image = velocity.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = attachments,
                                     .Depth = DepthAttachment{.Image = {.Image = depth.get()},
                                                              .Begin = Load::Clear,
                                                              .Clear = kQuarter}}));
  StageSubmission submission;
  Require(stage.Encode({},
                       {.Commands = commands.get(),
                        .Submission = submission,
                        .Area = {.Width = kWidth, .Height = kHeight},
                        .Clip = {.Width = kWidth, .Height = kHeight}}));
  Require(commands->EndRenderPass());
  Require(device.Submit(std::move(commands)));
  submission.Commit();
  Pixels expected{};
  for (size_t i = 0; i < kPixels; ++i) {
    expected[4 * i] = 0.5f;
    expected[4 * i + 1] = 1;
    expected[4 * i + 2] = kOneAndHalf;
    expected[4 * i + 3] = 1;
  }
  Compare(device, *hdr, expected);
  const auto bytes = Read(device, *depth);
  std::array<float, kPixels> depths{};
  std::memcpy(depths.data(), bytes.data(), sizeof(depths));
  CHECK(std::ranges::all_of(depths, [](float value) { return value == kQuarter; }),
        "sky never changes the accepted scene depth");
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  Pixels sky{};
  Pixels veil{};
  for (size_t i = 0; i < kPixels; ++i) {
    sky[4 * i] = kQuarter;
    sky[4 * i + 1] = 0.5f;
    sky[4 * i + 2] = kThreeQuarters;
    sky[4 * i + 3] = 1;
    for (size_t c = 0; c < 4; ++c) { veil[4 * i + c] = 1; }
  }
  auto table = Upload(device, Format::Rgba32Float, std::as_bytes(std::span(sky)), kRgbaRowBytes);
  auto trans = Upload(device, Format::Rgba32Float, std::as_bytes(std::span(veil)), kRgbaRowBytes);
  Composite(device, *sampler);
  Aerial(device, *sampler, *table, *trans);
  Sky(device, *sampler, *table, *trans);
  return Report();
}
