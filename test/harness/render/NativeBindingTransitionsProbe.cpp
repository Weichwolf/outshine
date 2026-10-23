#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Pixel = std::array<float, 4>;
static_assert(sizeof(Pixel) == 16);
constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 2;
constexpr uint32_t kRowBytes = 256;
constexpr uint64_t kOffset = 256;
constexpr uint64_t kBytes = uint64_t{kRowBytes} * kHeight;
constexpr uint8_t kChannelOne = 255;
constexpr Pixel kRed{1, 0, 0, 2};
constexpr Pixel kBlue{0, 0, 1, 2};
constexpr Pixel kGreen{0, 1, 0, 1};
constexpr Pixel kFirst{.25f, .5f, .75f, 1};
constexpr Pixel kSecond{.75f, .5f, .25f, 1};

std::unique_ptr<Texture> Target(Device &device) {
  return Require(
      device.CreateTexture({.Width = kWidth,
                            .Height = kHeight,
                            .Texels = Format::Rgba32Float,
                            .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
}

void CheckColumns(Device &device,
                  std::unique_ptr<Commands> commands,
                  Texture &image,
                  std::span<const Pixel, kWidth> expected) {
  auto readback = Require(device.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Require(commands->Copy(
      image, *readback, {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "binding transitions complete before their readback is inspected");
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = kBytes}));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      Pixel pixel{};
      std::memcpy(
          pixel.data(), bytes.data() + size_t{y} * kRowBytes + x * sizeof(Pixel), sizeof(pixel));
      CHECK(pixel == expected[x], "resource, range, sampler and uniform changes reach the GPU");
    }
  }
  Require(device.Unmap(*readback));
}

std::unique_ptr<Buffer> Input(Device &device, Pixel first, Pixel second) {
  std::array<Pixel, kOffset / sizeof(Pixel) + 1> values{};
  values.front() = first;
  values.back() = second;
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = sizeof(values), .Use = BufferUse::Storage, .Placement = Memory::Upload}));
  auto bytes = Require(device.MapWrite(*buffer, {.Bytes = sizeof(values)}));
  std::memcpy(bytes.data(), values.data(), sizeof(values));
  Require(device.Unmap(*buffer));
  return buffer;
}

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertexPath, const char *fragmentPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  return Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                .Fragment = ShaderCode{.Spirv = fragment},
                                                .Streams = {},
                                                .Attributes = {},
                                                .Colours = targets,
                                                .Depth = {},
                                                .Culling = Cull::None,
                                                .Label = {}}));
}

void DrawColumn(Commands &commands,
                GraphicsPipeline &pipeline,
                const Binding &binding,
                uint32_t column) {
  const std::array bindings{binding};
  Require(commands.Draw({.State = {.Pipeline = &pipeline,
                                   .Bindings = bindings,
                                   .Vertices = {},
                                   .Area = {.Width = kWidth, .Height = kHeight},
                                   .Clip = {.X = column, .Width = 1, .Height = kHeight}},
                         .Range = {.Count = 3}}));
}

void Case(Device &device, GraphicsPipeline &storage, GraphicsPipeline &uniform) {
  auto a = Input(device, {1, 0, 0, 0}, {});
  auto b = Input(device, {0, 0, 1, 0}, {0, 1, 0, 0});
  auto image = Target(device);
  auto commands = Require(device.Begin());
  std::array targets{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = {}}));
  Binding bound{.Set = 0,
                .Slot = 0,
                .Value = StorageBuffer{.Data = a.get(), .Range = {.Bytes = 2 * sizeof(Pixel)}}};
  DrawColumn(*commands, storage, bound, 0);
  DrawColumn(*commands, storage, bound, 1);
  std::get<StorageBuffer>(bound.Value).Data = b.get();
  DrawColumn(*commands, storage, bound, 2);
  std::get<StorageBuffer>(bound.Value).Range = {.Offset = kOffset, .Bytes = sizeof(Pixel)};
  DrawColumn(*commands, storage, bound, 3);
  Pixel mutableValue = kFirst;
  const Binding inlineValue{
      .Set = 0, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(mutableValue))}};
  DrawColumn(*commands, uniform, inlineValue, 4);
  mutableValue = kSecond;
  DrawColumn(*commands, uniform, inlineValue, 5);
  DrawColumn(*commands, storage, bound, 6);
  Require(commands->EndRenderPass());
  targets[0].Begin = Load::Load;
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = {}}));
  DrawColumn(*commands, storage, bound, 7);
  Require(commands->EndRenderPass());
  constexpr std::array expected{kRed, kRed, kBlue, kGreen, kFirst, kSecond, kGreen, kGreen};
  CheckColumns(device, std::move(commands), *image, expected);
}

std::unique_ptr<Texture>
TextureInput(Device &device, Commands &commands, const std::array<uint8_t, 8> &rgba) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = rgba.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto bytes = Require(device.MapWrite(*upload, {.Bytes = rgba.size()}));
  std::memcpy(bytes.data(), rgba.data(), rgba.size());
  Require(device.Unmap(*upload));
  auto image =
      Require(device.CreateTexture({.Width = 2,
                                    .Height = 1,
                                    .Texels = Format::Rgba8Unorm,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  Require(commands.Copy(*upload, *image, {.RowBytes = 8, .Region = {.Width = 2, .Height = 1}}));
  return image;
}

void TextureCase(Device &device, GraphicsPipeline &sampled) {
  auto commands = Require(device.Begin());
  auto a = TextureInput(
      device, *commands, {kChannelOne, 0, 0, kChannelOne, 0, 0, kChannelOne, kChannelOne});
  auto b = TextureInput(device, *commands, {0, 0, 0, kChannelOne, 0, kChannelOne, 0, kChannelOne});
  auto nearest = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  auto linear = Require(
      device.CreateSampler({.Min = Filter::Linear, .Mag = Filter::Linear, .Mip = Filter::Nearest}));
  auto image = Target(device);
  std::array targets{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = {}}));
  Binding bound{.Set = 0,
                .Slot = 0,
                .Value = SampledTexture{.Image = {.Image = a.get()}, .Sampling = nearest.get()}};
  DrawColumn(*commands, sampled, bound, 0);
  DrawColumn(*commands, sampled, bound, 1);
  auto &texture = std::get<SampledTexture>(bound.Value);
  texture.Image.Image = b.get();
  DrawColumn(*commands, sampled, bound, 2);
  texture.Sampling = linear.get();
  DrawColumn(*commands, sampled, bound, 3);
  texture.Image.Image = a.get();
  DrawColumn(*commands, sampled, bound, 4);
  texture.Sampling = nearest.get();
  DrawColumn(*commands, sampled, bound, 5);
  DrawColumn(*commands, sampled, bound, 6);
  Require(commands->EndRenderPass());
  targets[0].Begin = Load::Load;
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = {}}));
  DrawColumn(*commands, sampled, bound, 7);
  Require(commands->EndRenderPass());
  constexpr Pixel blue{0, 0, 1, 1};
  constexpr Pixel green{0, 1, 0, 1};
  constexpr Pixel halfGreen{0, .5f, 0, 1};
  constexpr Pixel purple{.5f, 0, .5f, 1};
  constexpr std::array expected{blue, blue, green, halfGreen, purple, blue, blue, blue};
  CheckColumns(device, std::move(commands), *image, expected);
}
}

int main(int argc, char **argv) {
  if ((argc != 5 && argc != 6) || (argc == 6 && std::string_view(argv[5]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 6}));
  auto storage = Pipeline(*device, argv[1], argv[2]);
  auto uniform = Pipeline(*device, argv[1], argv[3]);
  auto sampled = Pipeline(*device, argv[1], argv[4]);
  Case(*device, *storage, *uniform);
  TextureCase(*device, *sampled);
  TextureCase(*device, *sampled);
  Case(*device, *storage, *uniform);
  return Report();
}
