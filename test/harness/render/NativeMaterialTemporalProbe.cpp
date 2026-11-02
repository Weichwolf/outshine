#include "NativeMaterialCapture.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include "Gpu.h"
#include "FrameContext.h"
#include "TonemapStage.h"
#include "StageSubmission.h"
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace Render = outshine::Render;
using namespace Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint64_t kRowAlignment = 256;
constexpr size_t kProbeOffsetWord = 319, kHeaderWords = 324;

struct Images {
  std::unique_ptr<Texture> Scene, Motion, Depth, Display;
  std::array<std::unique_ptr<Texture>, 2> History;
  std::unique_ptr<Sampler> Sampling;
  std::unique_ptr<Buffer> Readback;
  uint32_t Width = 0, Height = 0;
  uint64_t RowBytes = 0, ImageBytes = 0;
};

Images Allocate(Device &device, uint32_t width, uint32_t height) {
  Images result;
  result.Width = width;
  result.Height = height;
  result.RowBytes = ((uint64_t{width} * 16 + kRowAlignment - 1) / kRowAlignment) * kRowAlignment;
  result.ImageBytes = result.RowBytes * height;
  const auto image = [&](Format format) {
    return Require(device.CreateTexture(
        {.Width = width,
         .Height = height,
         .Texels = format,
         .Use = TextureUse::ColourAttachment | TextureUse::Sampled | TextureUse::CopySource}));
  };
  result.Scene = image(Format::Rgba32Float);
  result.Motion = image(Format::Rgba32Float);
  result.Depth = image(Format::R32Float);
  result.Display = image(Format::Rgba32Float);
  for (auto &history : result.History) { history = image(Format::Rgba32Float); }
  result.Sampling = Require(device.CreateSampler({}));
  result.Readback = Require(device.CreateBuffer({.Bytes = 2 * result.ImageBytes,
                                                 .Use = BufferUse::CopyDestination,
                                                 .Placement = Memory::Readback}));
  return result;
}

void Clear(Device &device, Images &images) {
  auto commands = Require(device.Begin());
  const std::array colours{
      ColourAttachment{.Image = {.Image = images.History[0].get()}, .Begin = Load::Clear},
      ColourAttachment{.Image = {.Image = images.History[1].get()}, .Begin = Load::Clear},
      ColourAttachment{
          .Image = {.Image = images.Depth.get()}, .Begin = Load::Clear, .Clear = {1, 0, 0, 0}}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands->EndRenderPass());
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "initial material histories and plane depth clear completely");
}

void RecordMaterial(Commands &commands,
                    const GraphicsPipeline &pipeline,
                    const Buffer &inputs,
                    const Images &images,
                    uint32_t first) {
  const CaptureUniform parameters{
      .Count = images.Width * images.Height, .First = first, .Width = images.Width};
  const std::array bindings{
      Storage(0, 0, inputs),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&parameters, 1))}}};
  const std::array colours{
      ColourAttachment{.Image = {.Image = images.Scene.get()}, .Begin = Load::Clear},
      ColourAttachment{.Image = {.Image = images.Motion.get()}, .Begin = Load::Clear}};
  Require(commands.BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands.Draw({.State = {.Pipeline = &pipeline,
                                   .Bindings = bindings,
                                   .Vertices = {},
                                   .Area = {.Width = static_cast<float>(images.Width),
                                            .Height = static_cast<float>(images.Height)},
                                   .Clip = {.Width = images.Width, .Height = images.Height}},
                         .Range = {.Count = 3}}));
  Require(commands.EndRenderPass());
}

void SaveFrame(Device &device, const Images &images, std::ofstream &output) {
  const auto mapped = Require(device.MapRead(*images.Readback, {.Bytes = 2 * images.ImageBytes}));
  std::vector<CapturePixel> pixels(size_t{images.Width} * images.Height);
  for (uint32_t y = 0; y < images.Height; ++y) {
    for (uint32_t x = 0; x < images.Width; ++x) {
      auto &pixel = pixels[size_t{y} * images.Width + x];
      const auto offset = y * images.RowBytes + x * sizeof(pixel.First);
      std::memcpy(pixel.First.data(), mapped.data() + offset, sizeof(pixel.First));
      std::memcpy(
          pixel.Second.data(), mapped.data() + images.ImageBytes + offset, sizeof(pixel.Second));
    }
  }
  Require(device.Unmap(*images.Readback));
  const auto bytes = std::as_bytes(std::span(pixels));
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  CHECK(bool(output), "complete scene and resolved frame saved");
  if (!output) { Failed(); }
}

void Capture(Device &device,
             const GraphicsPipeline &pipeline,
             const Buffer &inputs,
             Images &images,
             uint32_t frames,
             const char *path) {
  const Render::Gpu gpu{.Device = &device,
                        .HdrFormat = Format::Rgba32Float,
                        .SurfaceFormat = Format::Rgba32Float,
                        .Width = static_cast<int>(images.Width),
                        .Height = static_cast<int>(images.Height),
                        .SceneColours = {},
                        .Uploads = nullptr};
  Render::TonemapStage tonemap;
  Require(tonemap.Configure(gpu,
                            {.Scene = images.Scene.get(),
                             .Depth = images.Depth.get(),
                             .Exact = images.Sampling.get(),
                             .Linear = Format::Rgba32Float},
                            {.Temporal = true}));
  Render::FrameContext context;
  for (size_t axis = 0; axis < 4; ++axis) {
    context.Mvp[axis * 5] = 1;
    context.PrevMvp[axis * 5] = 1;
  }
  std::ofstream output(path, std::ios::binary);
  CHECK(bool(output), "material sequence output opens");
  if (!output) { Failed(); }
  for (uint32_t frame = 0; frame < frames; ++frame) {
    auto commands = Require(device.Begin());
    RecordMaterial(*commands, pipeline, inputs, images, frame * images.Width * images.Height);
    tonemap.BindTemporal(
        {.History = images.History[frame % 2].get(), .Velocity = images.Motion.get()},
        {.WidthPx = static_cast<int>(images.Width), .HeightPx = static_cast<int>(images.Height)},
        {{0, 0}},
        frame != 0);
    const std::array colours{
        ColourAttachment{.Image = {.Image = images.History[(frame + 1) % 2].get()},
                         .Begin = Load::Clear},
        ColourAttachment{.Image = {.Image = images.Display.get()}, .Begin = Load::Clear}};
    Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
    Render::StageSubmission stages;
    const Render::PassRecording recording{.Commands = commands.get(),
                                          .Submission = stages,
                                          .Area = {.Width = static_cast<float>(images.Width),
                                                   .Height = static_cast<float>(images.Height)},
                                          .Clip = {.Width = images.Width, .Height = images.Height}};
    Require(tonemap.Encode(context, recording));
    Require(commands->EndRenderPass());
    for (size_t index = 0; index < 2; ++index) {
      const auto *image = index == 0 ? images.Scene.get() : images.History[(frame + 1) % 2].get();
      Require(commands->Copy(*image,
                             *images.Readback,
                             {.BufferOffset = index * images.ImageBytes,
                              .RowBytes = images.RowBytes,
                              .Region = {.Width = images.Width, .Height = images.Height}}));
    }
    const auto token = Require(device.Submit(std::move(commands)));
    stages.Commit();
    CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
          "material and production TAA frame completes within bounded wait");
    SaveFrame(device, images, output);
  }
  output.close();
  CHECK(bool(output), "material sequence output closes completely");
  std::println("device={} frames={} extent={}x{} pipeline=material+production-temporal",
               device.Limits().Name,
               frames,
               images.Width,
               images.Height);
}
}

int main(int argc, char *const *argv) {
  if (argc != 8) { return 2; }
  std::array<uint32_t, 3> shape{};
  for (size_t index = 0; index < shape.size(); ++index) {
    const std::string_view value(argv[index + 5]);
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), shape[index]);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        shape[index] == 0 || shape[index] > 128) {
      return 2;
    }
  }
  const auto words = ReadCaptureWords(argv[3]);
  const auto [width, height, frames] = shape;
  if (words.size() < kHeaderWords ||
      uint64_t{words[kProbeOffsetWord]} + uint64_t{width} * height * frames * 8 > words.size()) {
    return 2;
  }
  auto device = Require(CreateDevice({.Validation = true}));
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}},
                           ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  const auto pipeline =
      Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = Spirv(argv[1])},
                                              .Fragment = ShaderCode{.Spirv = Spirv(argv[2])},
                                              .Streams = {},
                                              .Attributes = {},
                                              .Colours = targets,
                                              .Depth = std::nullopt,
                                              .Culling = Cull::None,
                                              .Label = {}}));
  auto commands = Require(device->Begin());
  const auto inputs = Upload(*device, *commands, words);
  const auto token = Require(device->Submit(std::move(commands)));
  CHECK(Require(device->Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "immutable material packet is resident before the frame sequence");
  auto images = Allocate(*device, width, height);
  Clear(*device, images);
  Capture(*device, *pipeline, *inputs, images, frames, argv[4]);
  return Report();
}
