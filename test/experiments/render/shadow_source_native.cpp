#include "Check.h"
#include "NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 1280, kHeight = 720, kShadowSide = 8;
constexpr uint64_t kRowBytes = uint64_t{kWidth} * 4 * sizeof(float);
constexpr uint64_t kImageBytes = kRowBytes * kHeight;
constexpr size_t kMaterialBytes = 384, kLightBytes = 1344;
constexpr float kMaxLights = 16, kMaxRegions = 4;
using Parameters = std::array<std::byte, kMaterialBytes + kLightBytes>;

Parameters Read(const char *path) {
  Parameters data{};
  std::ifstream file(path, std::ios::binary);
  file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
  const bool valid = file.good() && file.peek() == std::ifstream::traits_type::eof();
  CHECK(valid, "the controlled material384 and lighting1344 payloads are complete");
  if (!valid) { Failed(); }
  std::array<float, 4> counts{};
  std::memcpy(counts.data(), data.data() + kMaterialBytes, sizeof(counts));
  const auto integer = [](float value, float maximum) {
    return std::isfinite(value) && value >= 0 && value <= maximum && value == std::trunc(value);
  };
  const bool bounded = integer(counts[0], kMaxLights) && integer(counts[2], counts[0]) &&
                       integer(counts[3], kMaxRegions) && std::isfinite(counts[1]);
  CHECK(bounded, "the private fixture cannot overrun the fixed shader light/region arrays");
  if (!bounded) { Failed(); }
  return data;
}

void Finish(Device &device, std::unique_ptr<Commands> commands, const char *phase, int iteration) {
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "the controlled native light draw completes within the offline bound");
  const auto timing = Require(device.SubmissionGpuTime(completion));
  CHECK(timing.Status == CompletionStatus::Ready && std::isfinite(timing.ElapsedNanoseconds) &&
            timing.ElapsedNanoseconds >= 0,
        "the completed submission has an actual finite GPU timestamp interval");
  std::println("gpu_submission phase={} iteration={} nanoseconds={}",
               phase,
               iteration,
               timing.ElapsedNanoseconds);
}

std::unique_ptr<Texture> Source(Device &device, uint32_t side) {
  return Require(device.CreateTexture({.Width = side,
                                       .Height = side,
                                       .Texels = Format::Rgba32Float,
                                       .Use = TextureUse::Sampled | TextureUse::ColourAttachment}));
}

void Prepare(Device &device, Texture &shadow, Texture &environment) {
  auto commands = Require(device.Begin());
  const std::array shadowTarget{ColourAttachment{
      .Image = {.Image = &shadow}, .Begin = Load::Clear, .Clear = {0.5f, 0, 0, 1}}};
  Require(commands->BeginRenderPass({.Colours = shadowTarget, .Depth = std::nullopt}));
  Require(commands->EndRenderPass());
  const std::array environmentTarget{
      ColourAttachment{.Image = {.Image = &environment}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = environmentTarget, .Depth = std::nullopt}));
  Require(commands->EndRenderPass());
  Finish(device, std::move(commands), "prepare", 0);
}

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertex, const char *fragment) {
  const auto v = Spirv(vertex);
  const auto f = Spirv(fragment);
  constexpr std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  auto pipeline = Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = v},
                                                         .Fragment = ShaderCode{.Spirv = f},
                                                         .Streams = {},
                                                         .Attributes = {},
                                                         .Colours = targets,
                                                         .Depth = std::nullopt,
                                                         .Culling = Cull::None,
                                                         .Label = "ExplicitShadowSource"}));
  const auto &bindings = pipeline->Layout().Bindings;
  CHECK(bindings.size() == 4, "the actual light kernel keeps two samplers and two uniforms");
  for (const auto &binding : bindings) {
    const auto &resource = binding.Resource;
    if (resource.Set != 3) { continue; }
    CHECK(resource.Kind == BindingKind::Uniform &&
              resource.MinimumBytes == (resource.Slot == 0 ? kMaterialBytes : kLightBytes),
          "actual material and light host/GPU layouts agree");
  }
  return pipeline;
}

void DrawCase(Device &device,
              const GraphicsPipeline &pipeline,
              const Parameters &parameters,
              const char *outputPath) {
  auto shadow = Source(device, kShadowSide);
  auto environment = Source(device, 2);
  auto image =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto sampler = Require(device.CreateSampler({}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = kImageBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Prepare(device, *shadow, *environment);
  const std::span data(parameters);
  const std::array bindings{
      Binding{.Set = 2,
              .Slot = 0,
              .Value = SampledTexture{.Image = {.Image = shadow.get()}, .Sampling = sampler.get()}},
      Binding{.Set = 2,
              .Slot = 1,
              .Value =
                  SampledTexture{.Image = {.Image = environment.get()}, .Sampling = sampler.get()}},
      Binding{.Set = 3, .Slot = 0, .Value = Uniform{data.first(kMaterialBytes)}},
      Binding{.Set = 3, .Slot = 1, .Value = Uniform{data.subspan(kMaterialBytes)}}};
  const std::array targets{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  for (int iteration = 0; iteration < 4; ++iteration) {
    auto commands = Require(device.Begin());
    Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
    Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                      .Bindings = bindings,
                                      .Vertices = {},
                                      .Area = {.Width = kWidth, .Height = kHeight},
                                      .Clip = {.Width = kWidth, .Height = kHeight}},
                            .Range = {.Count = 3}}));
    Require(commands->EndRenderPass());
    Require(
        commands->Copy(*image,
                       *readback,
                       {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
    Finish(device, std::move(commands), "draw-readback", iteration);
  }
  const auto pixels = Require(device.MapRead(*readback, {.Bytes = kImageBytes}));
  std::ofstream file(outputPath, std::ios::binary);
  file.write(reinterpret_cast<const char *>(pixels.data()),
             static_cast<std::streamsize>(pixels.size()));
  CHECK(file.good(), "the complete linear native light image is written");
  Require(device.Unmap(*readback));
}
}

int main(int argc, char **argv) {
  if (argc != 5) { return 2; }
  const auto parameters = Read(argv[3]);
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = true}));
  const auto pipeline = Pipeline(*device, argv[1], argv[2]);
  std::println("device={} width={} height={}", device->Limits().Name, kWidth, kHeight);
  DrawCase(*device, *pipeline, parameters, argv[4]);
  return Report();
}
