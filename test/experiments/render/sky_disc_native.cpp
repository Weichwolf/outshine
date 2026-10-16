#include "Atmosphere.h"
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
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Pixel = std::array<float, 4>;
constexpr uint32_t kWidth = 1280, kHeight = 720;
constexpr uint32_t kLutWidth = 256, kLutHeight = 64;
constexpr uint64_t kImageBytes = uint64_t{kWidth} * kHeight * sizeof(Pixel);
constexpr uint64_t kHalfPixelBytes = 4 * sizeof(uint16_t);
constexpr uint64_t kLutBytes = uint64_t{kLutWidth} * kLutHeight * kHalfPixelBytes;
constexpr size_t kSkyFrameBytes = 192, kSkyBasisBytes = 80, kEyeRadiusOffset = 92;
constexpr size_t kBottomRadiusOffset = 96, kMediumOffset = 112;
constexpr float kSunHalfAngleRad = 0.00465f;

struct alignas(16) Parameters {
  Pixel Right, Up, Forward, WorldUp, SunDirection;
  std::array<float, 2> TanHalf;
  float Illuminance, EyeRadiusKm;
  float BottomRadiusKm, TopRadiusKm, SunHalfAngleRad, Pad;
  Medium Air;
};

static_assert(sizeof(Parameters) == kSkyFrameBytes && alignof(Parameters) == 16);
static_assert(offsetof(Parameters, TanHalf) == kSkyBasisBytes &&
              offsetof(Parameters, EyeRadiusKm) == kEyeRadiusOffset);
static_assert(offsetof(Parameters, BottomRadiusKm) == kBottomRadiusOffset &&
              offsetof(Parameters, Air) == kMediumOffset);

struct Scenario {
  const char *Name;
  float HeightKm;
  float CosSun;
};

constexpr std::array kScenarios{
    Scenario{.Name = "ground-high", .HeightKm = 0.02f, .CosSun = 0.8f},
    Scenario{.Name = "ground-low", .HeightKm = 0.02f, .CosSun = 0.03f},
    Scenario{.Name = "top", .HeightKm = 100.0f, .CosSun = -0.1f},
    Scenario{.Name = "orbit-shallow", .HeightKm = 110.0f, .CosSun = -0.1f},
    Scenario{.Name = "orbit-grazing", .HeightKm = 110.0f, .CosSun = -0.17f},
    Scenario{.Name = "orbit-edge", .HeightKm = 110.0f, .CosSun = -0.183f},
    Scenario{.Name = "high-orbit", .HeightKm = 600.0f, .CosSun = -0.4f},
    Scenario{.Name = "high-orbit-miss", .HeightKm = 600.0f, .CosSun = -0.1f}};

Parameters Frame(Scenario scenario) {
  const float cosine = scenario.CosSun;
  const float horizontal = std::sqrt(1.0f - cosine * cosine);
  constexpr float tangent = 0.00872686779f;
  return {.Right = {0, 0, 1, 0},
          .Up = {-cosine, horizontal, 0, 0},
          .Forward = {horizontal, cosine, 0, 0},
          .WorldUp = {0, 1, 0, 0},
          .SunDirection = {horizontal, cosine, 0, 0},
          .TanHalf = {tangent * float(kWidth) / float(kHeight), tangent},
          .Illuminance = 1,
          .EyeRadiusKm = kEarthAir.BottomRadiusKm + scenario.HeightKm,
          .BottomRadiusKm = kEarthAir.BottomRadiusKm,
          .TopRadiusKm = kEarthAir.TopRadiusKm,
          .SunHalfAngleRad = kSunHalfAngleRad,
          .Pad = 0,
          .Air = kEarthAir};
}

void Finish(Device &device, std::unique_ptr<Commands> commands) {
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "native sky operations complete within the offline bound");
}

void Write(const std::string &path, std::span<const std::byte> bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  CHECK(file.good(), "native sky capture is written completely");
}

struct Images {
  std::unique_ptr<Texture> Sky, Veil;
  std::array<std::unique_ptr<Texture>, 2> Output;
  std::unique_ptr<Buffer> Readback, LutReadback;
  std::unique_ptr<Sampler> Linear;

  explicit Images(Device &device) {
    Sky =
        Require(device.CreateTexture({.Width = 2,
                                      .Height = 2,
                                      .Texels = Format::Rgba32Float,
                                      .Use = TextureUse::ColourAttachment | TextureUse::Sampled}));
    Veil = Require(device.CreateTexture(
        {.Width = kLutWidth,
         .Height = kLutHeight,
         .Texels = Format::Rgba16Float,
         .Use = TextureUse::Storage | TextureUse::Sampled | TextureUse::CopySource}));
    for (auto &image : Output) {
      image = Require(
          device.CreateTexture({.Width = kWidth,
                                .Height = kHeight,
                                .Texels = Format::Rgba32Float,
                                .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    }
    Readback = Require(device.CreateBuffer({.Bytes = 2 * kImageBytes,
                                            .Use = BufferUse::CopyDestination,
                                            .Placement = Memory::Readback}));
    LutReadback = Require(device.CreateBuffer(
        {.Bytes = kLutBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
    Linear = Require(device.CreateSampler({}));
  }
};

void Generate(Device &device,
              const ComputePipeline &pipeline,
              Images &images,
              const std::string &folder) {
  const Medium medium = kEarthAir;
  auto commands = Require(device.Begin());
  const std::array targets{ColourAttachment{.Image = {.Image = images.Sky.get()},
                                            .Begin = Load::Clear,
                                            .Clear = {0.125f, 0.25f, 0.5f, 1.0f}}};
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
  Require(commands->EndRenderPass());
  const std::array bindings{
      Binding{.Set = 1, .Slot = 0, .Value = StorageTexture{.Image = {.Image = images.Veil.get()}}},
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&medium, 1))}}};
  Require(commands->Dispatch(pipeline, bindings, {.X = kLutWidth / 8, .Y = kLutHeight / 8}));
  Require(commands->Copy(*images.Veil,
                         *images.LutReadback,
                         {.RowBytes = kLutWidth * kHalfPixelBytes,
                          .Region = {.Width = kLutWidth, .Height = kLutHeight}}));
  Finish(device, std::move(commands));
  const auto mapped = Require(device.MapRead(*images.LutReadback, {.Bytes = kLutBytes}));
  Write(folder + "/veil.rgba16", mapped);
  Require(device.Unmap(*images.LutReadback));
}

void FloatVeil(Device &device, Images &images, const char *path) {
  std::vector<std::byte> bytes(2 * kLutBytes);
  std::ifstream file(path, std::ios::binary);
  file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  const bool valid = file.good() && file.peek() == std::ifstream::traits_type::eof();
  CHECK(valid, "the optional FP32 control contains exactly the promoted LUT texels");
  if (!valid) { Failed(); }
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*upload));
  images.Veil =
      Require(device.CreateTexture({.Width = kLutWidth,
                                    .Height = kLutHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*upload,
                         *images.Veil,
                         {.RowBytes = kLutWidth * sizeof(Pixel),
                          .Region = {.Width = kLutWidth, .Height = kLutHeight}}));
  Finish(device, std::move(commands));
}

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertex, const char *fragment) {
  const auto vertexCode = Spirv(vertex);
  const auto fragmentCode = Spirv(fragment);
  constexpr std::array colours{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}},
                               ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  auto result =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertexCode},
                                             .Fragment = ShaderCode{.Spirv = fragmentCode},
                                             .Streams = {},
                                             .Attributes = {},
                                             .Colours = colours,
                                             .Depth = std::nullopt,
                                             .Culling = Cull::None,
                                             .Label = "PhysicalSolarDisc"}));
  size_t uniforms = 0;
  for (const auto &binding : result->Layout().Bindings) {
    if (binding.Resource.Set == 3 && binding.Resource.Slot == 0) {
      CHECK(binding.Resource.Kind == BindingKind::Uniform &&
                binding.Resource.MinimumBytes == kSkyFrameBytes,
            "the actual sky shader retains its uniform192 contract");
      ++uniforms;
    }
  }
  CHECK(uniforms == 1 && result->Layout().Bindings.size() == 3,
        "the actual sky shader consumes its unchanged two samplers and one uniform");
  return result;
}

void DrawSky(Device &device,
             const GraphicsPipeline &pipeline,
             Images &images,
             Scenario scenario,
             const std::string &folder) {
  const auto frame = Frame(scenario);
  const std::array bindings{
      Binding{.Set = 2,
              .Slot = 0,
              .Value = SampledTexture{.Image = {.Image = images.Sky.get()},
                                      .Sampling = images.Linear.get()}},
      Binding{.Set = 2,
              .Slot = 1,
              .Value = SampledTexture{.Image = {.Image = images.Veil.get()},
                                      .Sampling = images.Linear.get()}},
      Binding{.Set = 3, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&frame, 1))}}};
  const std::array targets{
      ColourAttachment{.Image = {.Image = images.Output[0].get()}, .Begin = Load::Clear},
      ColourAttachment{.Image = {.Image = images.Output[1].get()}, .Begin = Load::Clear}};
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = kWidth, .Height = kHeight},
                                    .Clip = {.Width = kWidth, .Height = kHeight}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  for (size_t i = 0; i < images.Output.size(); ++i) {
    Require(commands->Copy(*images.Output[i],
                           *images.Readback,
                           {.BufferOffset = i * kImageBytes,
                            .RowBytes = kWidth * sizeof(Pixel),
                            .Region = {.Width = kWidth, .Height = kHeight}}));
  }
  Finish(device, std::move(commands));
  const auto mapped = Require(device.MapRead(*images.Readback, {.Bytes = 2 * kImageBytes}));
  const auto prefix = folder + "/" + scenario.Name;
  Write(prefix + ".rgba32", mapped.first(kImageBytes));
  Write(prefix + ".velocity32", mapped.subspan(kImageBytes));
  Write(prefix + ".parameters", std::as_bytes(std::span(&frame, 1)));
  Require(device.Unmap(*images.Readback));
}
}

int main(int argc, char **argv) {
  if (argc != 5 && argc != 6) { return 2; }
  auto device = Require(CreateDevice({.Validation = true}));
  std::println("device={} width={} height={} scenarios={}",
               device->Limits().Name,
               kWidth,
               kHeight,
               kScenarios.size());
  const auto transmittance = Require(device->CreateComputePipeline(Spirv(argv[3])));
  const auto pipeline = Pipeline(*device, argv[1], argv[2]);
  Images images(*device);
  Generate(*device, *transmittance, images, argv[4]);
  if (argc == 6) { FloatVeil(*device, images, argv[5]); }
  for (const auto &scenario : kScenarios) {
    DrawSky(*device, *pipeline, images, scenario, argv[4]);
  }
  return Report();
}
