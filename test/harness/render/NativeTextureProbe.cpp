#include "NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <string_view>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 8, kHeight = 4, kRowBytes = 256;

struct alignas(16) Parameters {
  std::array<float, 4> Tint{2, 1, 4, 1};
  std::array<float, 4> Shift{.5f, .5f, 0, 0};
};

static_assert(sizeof(Parameters) == 32 && alignof(Parameters) == 16 &&
              offsetof(Parameters, Shift) == 16);

std::unique_ptr<Buffer> Pixels(Device &device, bool half) {
  constexpr size_t bytes = size_t{kRowBytes} * kHeight;
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*buffer, {.Bytes = bytes}));
  std::memset(mapped.data(), 0, bytes);
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      auto *const at = mapped.data() + size_t{y} * kRowBytes;
      if (half) {
        const std::array<uint16_t, 4> rgba{
            static_cast<uint16_t>(x % 2 == 0 ? 0 : 0x3c00), 0x3800, 0x3400, 0x3c00};
        std::memcpy(at + x * sizeof(rgba), rgba.data(), sizeof(rgba));
      } else {
        const std::array<float, 4> rgba{static_cast<float>(x + y * kWidth), .5f, .25f, 1};
        std::memcpy(at + x * sizeof(rgba), rgba.data(), sizeof(rgba));
      }
    }
  }
  Require(device.Unmap(*buffer));
  return buffer;
}

void Sample(Device &device, const ComputePipeline &pipeline, bool linear) {
  const auto format = linear ? Format::Rgba16Float : Format::Rgba32Float;
  CHECK(device.Supports(format, TextureUse::Sampled | TextureUse::CopyDestination, linear),
        "the requested input format/filter combination is supported");
  auto source =
      Require(device.CreateTexture({.Width = kWidth * 2,
                                    .Height = kHeight * 2,
                                    .Mips = 3,
                                    .Layers = 3,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = format,
                                    .Use = TextureUse::CopyDestination | TextureUse::Sampled}));
  auto output = Require(device.CreateTexture({.Width = kWidth,
                                              .Height = kHeight,
                                              .Type = TextureType::Image2DArray,
                                              .Texels = Format::Rgba32Float,
                                              .Use = TextureUse::Storage | TextureUse::Sampled}));
  auto intermediate =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Storage | TextureUse::CopySource}));
  auto copied =
      Require(device.CreateTexture({.Width = kWidth + 2,
                                    .Height = kHeight + 2,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::CopyDestination | TextureUse::CopySource}));
  auto sampler = Require(device.CreateSampler({.Min = linear ? Filter::Linear : Filter::Nearest,
                                               .Mag = linear ? Filter::Linear : Filter::Nearest,
                                               .Mip = Filter::Nearest}));
  auto nearest = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  auto upload = Pixels(device, linear);
  auto readback = Require(device.CreateBuffer({.Bytes = uint64_t{kRowBytes} * kHeight,
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*upload,
                         *source,
                         {.RowBytes = kRowBytes,
                          .Region = {.Mip = 1, .Layer = 1, .Width = kWidth, .Height = kHeight}}));
  Parameters parameters;
  if (linear) { parameters.Shift[0] = 1.0f; }
  const std::array bindings{
      Binding{.Set = 0,
              .Slot = 0,
              .Value =
                  SampledTexture{.Image = {.Image = source.get(), .FirstMip = 1, .FirstLayer = 1},
                                 .Sampling = sampler.get()}},
      Binding{.Set = 1, .Slot = 0, .Value = StorageTexture{.Image = {.Image = output.get()}}},
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&parameters, 1))}}};
  Require(commands->Dispatch(pipeline, bindings, {}));
  parameters.Tint.fill(1);
  parameters.Shift = {.5f, .5f, 0, 0};
  auto chained = bindings;
  std::get<SampledTexture>(chained[0].Value) = {.Image = {.Image = output.get()},
                                                .Sampling = nearest.get()};
  std::get<StorageTexture>(chained[1].Value).Image = {.Image = intermediate.get()};
  Require(commands->Dispatch(pipeline, chained, {}));
  parameters.Tint.fill(0);
  Require(commands->Copy(*intermediate,
                         {.Width = kWidth, .Height = kHeight},
                         *copied,
                         {.X = 1, .Y = 1, .Width = kWidth, .Height = kHeight}));
  Require(commands->Copy(
      *copied,
      *readback,
      {.RowBytes = kRowBytes, .Region = {.X = 1, .Y = 1, .Width = kWidth, .Height = kHeight}}));
  source.reset();
  output.reset();
  intermediate.reset();
  copied.reset();
  sampler.reset();
  nearest.reset();
  upload.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "texture transfers and dispatch complete within the diagnostic budget");
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = uint64_t{kRowBytes} * kHeight}));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      std::array<float, 4> actual{};
      std::memcpy(
          actual.data(), bytes.data() + size_t{y} * kRowBytes + x * sizeof(actual), sizeof(actual));
      float red = 2.0f * static_cast<float>(x + y * kWidth) + 2.0f;
      if (linear) { red = x == kWidth - 1 ? 4.0f : 3.0f; }
      const std::array expected{red, .5f, 1.0f, 1.0f};
      for (size_t channel = 0; channel < actual.size(); ++channel) {
        CHECK(actual[channel] == expected[channel],
              "texture view/filter/store/copy matches the CPU image");
      }
    }
  }
  Require(device.Unmap(*readback));
}

void Invalid(Device &device) {
  CHECK(!device.Supports(Format::Rgba8Srgb, TextureUse::Storage, false),
        "sRGB storage writes are not advertised");
  CHECK(!device.Supports(Format::Depth32Float, TextureUse::ColourAttachment, false),
        "depth data cannot be a colour attachment");
  const auto bad =
      device.CreateTexture({.Width = 8, .Height = 8, .Mips = 5, .Use = TextureUse::Sampled});
  CHECK(!bad && bad.error().Kind == ErrorKind::Invalid,
        "mip count is bounded by actual dimensions");
  const std::array ranges{SamplerDescription{.MinLod = -1},
                          SamplerDescription{.MinLod = 2, .MaxLod = 1},
                          SamplerDescription{.MinLod = std::numeric_limits<float>::quiet_NaN()},
                          SamplerDescription{.MaxLod = std::numeric_limits<float>::infinity()}};
  for (const auto &range : ranges) {
    const auto sampler = device.CreateSampler(range);
    CHECK(!sampler && sampler.error().Kind == ErrorKind::Invalid,
          "invalid sampler LOD ranges are rejected before the driver");
  }
}

std::unique_ptr<Texture> MipColours(Device &device, Commands &commands) {
  constexpr uint64_t bytes = uint64_t{kRowBytes} * kHeight * 2;
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes}));
  std::memset(mapped.data(), 0, bytes);
  constexpr std::array colours{std::array{.25f, .5f, .75f, 1.f}, std::array{.75f, .25f, .5f, 1.f}};
  for (uint32_t level = 0; level < colours.size(); ++level) {
    for (uint32_t y = 0; y < (kHeight >> level); ++y) {
      for (uint32_t x = 0; x < (kWidth >> level); ++x) {
        const uint64_t offset = uint64_t{level * kHeight + y} * kRowBytes + x * sizeof(colours[0]);
        std::memcpy(mapped.data() + offset, colours[level].data(), sizeof(colours[0]));
      }
    }
  }
  Require(device.Unmap(*upload));
  auto image =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Mips = 2,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::CopyDestination | TextureUse::Sampled}));
  for (uint32_t level = 0; level < 2; ++level) {
    Require(commands.Copy(
        *upload,
        *image,
        {.BufferOffset = uint64_t{level} * kRowBytes * kHeight,
         .RowBytes = kRowBytes,
         .Region = {.Mip = level, .Width = kWidth >> level, .Height = kHeight >> level}}));
  }
  return image;
}

void LodClamps(Device &device, const ComputePipeline &pipeline) {
  auto uploads = Require(device.Begin());
  const auto image = MipColours(device, *uploads);
  const auto uploaded = Require(device.Submit(std::move(uploads)));
  CHECK(Require(device.Wait(uploaded, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "both distinct source mip colours are uploaded before sampling");
  const auto output =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Storage | TextureUse::CopySource}));
  constexpr uint64_t bytes = uint64_t{kRowBytes} * kHeight;
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  for (uint32_t test = 0; test < 4; ++test) {
    auto commands = Require(device.Begin());
    auto sampler = Require(device.CreateSampler({.Min = Filter::Nearest,
                                                 .Mag = Filter::Nearest,
                                                 .Mip = Filter::Nearest,
                                                 .MinLod = test == 2 ? 1.f : 0.f,
                                                 .MaxLod = test == 3 ? 0.f : 1.f}));
    Parameters parameters;
    parameters.Shift[2] = static_cast<float>(test % 2);
    const std::array bindings{
        Binding{.Set = 0,
                .Slot = 0,
                .Value = SampledTexture{.Image = {.Image = image.get(), .Mips = 2},
                                        .Sampling = sampler.get()}},
        Binding{.Set = 1, .Slot = 0, .Value = StorageTexture{.Image = {.Image = output.get()}}},
        Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&parameters, 1))}}};
    Require(commands->Dispatch(pipeline, bindings, {.X = 1, .Y = 1}));
    sampler.reset();
    Require(
        commands->Copy(*output,
                       *readback,
                       {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
    const auto completion = Require(device.Submit(std::move(commands)));
    CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
          "sampler LOD draws finish with their outer sampler owner released");
    const auto result = Require(device.MapRead(*readback, {.Bytes = bytes}));
    const std::array expected =
        test == 1 || test == 2 ? std::array{2.5f, .25f, 2.f, 1.f} : std::array{1.5f, .5f, 3.f, 1.f};
    for (uint32_t y = 0; y < kHeight; ++y) {
      for (uint32_t x = 0; x < kWidth; ++x) {
        std::array<float, 4> rgba{};
        std::memcpy(
            rgba.data(), result.data() + size_t{y} * kRowBytes + x * sizeof(rgba), sizeof(rgba));
        for (uint32_t channel = 0; channel < rgba.size(); ++channel) {
          CHECK(rgba[channel] == expected[channel],
                "sampler LOD clamps select the expected distinct mip colour");
        }
      }
    }
    Require(device.Unmap(*readback));
  }
}

struct alignas(16) Reducing {
  std::array<uint32_t, 4> Source{35, 19, 0, 0};
  std::array<uint32_t, 4> Wide{18, 9, 5, 3}, High{10, 5, 3, 2}, At{0, 180, 225, 240};
};

static_assert(sizeof(Reducing) == 64 && alignof(Reducing) == 16);

float Depth(uint32_t x, uint32_t y) {
  constexpr float levels = 256;
  return static_cast<float>((x * 7 + y * 13) % 256) / levels;
}

void Pyramid(Device &device, const char *path) {
  const Reducing parameters;
  constexpr uint32_t values = 246;
  constexpr uint32_t rowBytes = 256;
  const uint64_t bytes = uint64_t{rowBytes} * parameters.Source[1];
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes}));
  for (uint32_t y = 0; y < parameters.Source[1]; ++y) {
    for (uint32_t x = 0; x < parameters.Source[0]; ++x) {
      const auto depth = Depth(x, y);
      std::memcpy(mapped.data() + size_t{y} * rowBytes + x * sizeof(float), &depth, sizeof(depth));
    }
  }
  Require(device.Unmap(*upload));
  auto texture =
      Require(device.CreateTexture({.Width = parameters.Source[0],
                                    .Height = parameters.Source[1],
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::CopyDestination | TextureUse::Sampled}));
  const auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  const auto output = Require(device.CreateBuffer(
      {.Bytes = values * sizeof(float), .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto readback = Require(device.CreateBuffer({.Bytes = values * sizeof(float),
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  const auto pipeline = Require(device.CreateComputePipeline(Spirv(path)));
  auto commands = Require(device.Begin());
  Require(
      commands->Copy(*upload,
                     *texture,
                     {.RowBytes = rowBytes,
                      .Region = {.Width = parameters.Source[0], .Height = parameters.Source[1]}}));
  const std::array bindings{
      Binding{.Set = 0,
              .Slot = 0,
              .Value =
                  SampledTexture{.Image = {.Image = texture.get()}, .Sampling = sampler.get()}},
      Storage(1, 0, *output),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&parameters, 1))}}};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 3, .Y = 2}));
  Require(commands->Copy(*output, *readback, {.Bytes = values * sizeof(float)}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "production Hi-Z finishes through the common GPU API");
  const auto result = Require(device.MapRead(*readback, {.Bytes = values * sizeof(float)}));
  for (uint32_t level = 0; level < 4; ++level) {
    const uint32_t side = 2u << level;
    for (uint32_t y = 0; y < parameters.High[level]; ++y) {
      for (uint32_t x = 0; x < parameters.Wide[level]; ++x) {
        float expected = 1;
        for (uint32_t dy = 0; dy < side; ++dy) {
          for (uint32_t dx = 0; dx < side; ++dx) {
            expected = std::min(expected,
                                Depth(std::min(x * side + dx, parameters.Source[0] - 1),
                                      std::min(y * side + dy, parameters.Source[1] - 1)));
          }
        }
        float actual = 0;
        const auto index = parameters.At[level] + y * parameters.Wide[level] + x;
        std::memcpy(&actual, result.data() + index * sizeof(float), sizeof(actual));
        CHECK(actual == expected, "common-API depth pyramid agrees with independent CPU reduction");
      }
    }
  }
  Require(device.Unmap(*readback));
}

}

int main(int argc, char *const *argv) {
  if ((argc != 3 && argc != 4) || (argc == 4 && std::string_view(argv[3]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 4}));
  const auto pipeline = Require(device->CreateComputePipeline(Spirv(argv[1])));
  Invalid(*device);
  Sample(*device, *pipeline, false);
  Sample(*device, *pipeline, true);
  LodClamps(*device, *pipeline);
  Pyramid(*device, argv[2]);
  return Report();
}
