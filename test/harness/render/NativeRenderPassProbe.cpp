#include "NativeGpuCheck.h"
#include "Check.h"
#include "Device.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <limits>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 8, kHeight = 4, kRowBytes = 256;
constexpr uint64_t kImageBytes = uint64_t{kRowBytes} * kHeight;
constexpr float kRedStep = .125f, kGreen = .25f, kDepth = .375f;

void Invalid(Result<void> result, const char *reason) {
  CHECK(!result && result.error().Kind == ErrorKind::Invalid, reason);
}

void Clears(Device &device) {
  std::array<std::unique_ptr<Texture>, 5> images;
  std::array<ColourAttachment, 5> colours;
  for (size_t i = 0; i < images.size(); ++i) {
    images[i] = Require(
        device.CreateTexture({.Width = kWidth * 2,
                              .Height = kHeight * 2,
                              .Mips = 2,
                              .Layers = 2,
                              .Type = TextureType::Image2DArray,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    colours[i] = {.Image = {.Image = images[i].get(), .FirstMip = 1, .FirstLayer = 1},
                  .Begin = Load::Clear,
                  .Clear = {static_cast<float>(i) * kRedStep, kGreen, .5f, 1}};
  }
  colours.back().Clear[1] = std::numeric_limits<float>::infinity();
  auto depth =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  auto pixels = Require(device.CreateBuffer({.Bytes = kImageBytes * 6,
                                             .Use = BufferUse::CopyDestination,
                                             .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  RenderPassDescription pass{
      .Colours = colours,
      .Depth =
          DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear, .Clear = kDepth}};
  Require(commands->BeginRenderPass(pass));
  Invalid(commands->BeginRenderPass(pass), "nested render passes are rejected");
  bool wrongThread = false;
  std::thread([&] {
    const auto result = commands->EndRenderPass();
    wrongThread = !result && result.error().Kind == ErrorKind::Invalid;
  }).join();
  CHECK(wrongThread, "another thread cannot terminate an open pass");
  Invalid(commands->Copy(
              *images[0], *pixels, {.RowBytes = kRowBytes, .Region = {.Width = 1, .Height = 1}}),
          "a texture transfer cannot silently terminate a render pass");
  Require(commands->EndRenderPass());
  Invalid(commands->EndRenderPass(), "a render pass cannot be ended twice");
  for (auto &colour : colours) { colour.Begin = Load::Load; }
  pass.Depth->Begin = Load::Load;
  Require(commands->BeginRenderPass(pass));
  Require(commands->EndRenderPass());
  for (size_t i = 0; i < images.size(); ++i) {
    Require(commands->Copy(*images[i],
                           *pixels,
                           {.BufferOffset = i * kImageBytes,
                            .RowBytes = kRowBytes,
                            .Region = {.Mip = 1, .Layer = 1, .Width = kWidth, .Height = kHeight}}));
  }
  Require(commands->Copy(*depth,
                         *pixels,
                         {.BufferOffset = 5 * kImageBytes,
                          .RowBytes = kRowBytes,
                          .Region = {.Width = kWidth, .Height = kHeight}}));
  for (auto &image : images) { image.reset(); }
  depth.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "MRT clear/load/store survives release of every attachment owner before submission");
  const auto mapped = Require(device.MapRead(*pixels, {.Bytes = kImageBytes * 6}));
  for (size_t image = 0; image < 6; ++image) {
    for (uint32_t y = 0; y < kHeight; ++y) {
      for (uint32_t x = 0; x < kWidth; ++x) {
        const size_t channels = image == 5 ? 1 : 4;
        std::array<float, 4> value{};
        std::memcpy(value.data(),
                    mapped.data() + image * kImageBytes + size_t{y} * kRowBytes +
                        x * channels * sizeof(float),
                    channels * sizeof(float));
        for (size_t c = 0; c < channels; ++c) {
          const float expected = image == 5 ? kDepth : colours[image].Clear[c];
          CHECK(value[c] == expected,
                "selected mip/layer preserves each cleared colour/depth texel");
        }
      }
    }
  }
  Require(device.Unmap(*pixels));
}

void InvalidAttachments(Device &device) {
  const auto image = Require(device.CreateTexture({.Width = kWidth,
                                                   .Height = kHeight,
                                                   .Texels = Format::Rgba8Unorm,
                                                   .Use = TextureUse::ColourAttachment}));
  const auto small = Require(device.CreateTexture({.Width = 1,
                                                   .Height = 1,
                                                   .Texels = Format::Rgba8Unorm,
                                                   .Use = TextureUse::ColourAttachment}));
  const auto source = Require(device.CreateTexture({.Width = kWidth,
                                                    .Height = kHeight,
                                                    .Texels = Format::Rgba8Unorm,
                                                    .Use = TextureUse::Sampled}));
  auto commands = Require(device.Begin());
  const ColourAttachment colour{.Image = {.Image = image.get()}, .Begin = Load::Clear};
  const auto reject = [&](std::span<const ColourAttachment> colours) {
    Invalid(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}),
            "invalid attachment set is rejected");
  };
  reject({});
  std::array<ColourAttachment, 9> tooMany;
  tooMany.fill(colour);
  reject(tooMany);
  std::array repeated{colour, colour};
  reject(repeated);
  repeated[1].Image.Image = small.get();
  reject(repeated);
  for (const TextureView view : std::array{TextureView{},
                                           TextureView{.Image = source.get()},
                                           TextureView{.Image = image.get(), .Mips = 2},
                                           TextureView{.Image = image.get(), .Layers = 2},
                                           TextureView{.Image = image.get(), .FirstMip = 1},
                                           TextureView{.Image = image.get(), .FirstLayer = 1}}) {
    const std::array targets{ColourAttachment{.Image = view}};
    reject(targets);
  }
  auto invalid = colour;
  invalid.Begin = static_cast<Load>(-1);
  reject(std::span(&invalid, 1));
  invalid = colour;
  invalid.End = static_cast<Store>(-1);
  reject(std::span(&invalid, 1));
  Invalid(commands->BeginRenderPass(
              {.Colours = {}, .Depth = DepthAttachment{.Image = {.Image = image.get()}}}),
          "a colour texture cannot masquerade as a depth target");
  const std::array targets{colour};
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
  auto submitted = device.Submit(std::move(commands));
  CHECK(!submitted && submitted.error().Kind == ErrorKind::Invalid,
        "submission of an open pass is rejected and cancels its native encoder");
  commands = Require(device.Begin());
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
  commands.reset();
  commands = Require(device.Begin());
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
  Require(commands->EndRenderPass());
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "explicit cancellation and invalid submission leave later render passes usable");
}

void DepthActions(Device &device) {
  const auto depth = Require(device.CreateTexture({.Width = kWidth,
                                                   .Height = kHeight,
                                                   .Texels = Format::Depth32Float,
                                                   .Use = TextureUse::DepthAttachment}));
  auto commands = Require(device.Begin());
  RenderPassDescription pass{.Colours = {},
                             .Depth = DepthAttachment{.Image = {.Image = depth.get()}}};
  for (const float clear : std::array{-1.f,
                                      2.f,
                                      std::numeric_limits<float>::infinity(),
                                      std::numeric_limits<float>::quiet_NaN()}) {
    pass.Depth->Clear = clear;
    Invalid(commands->BeginRenderPass(pass), "depth clears must be finite and within zero to one");
  }
  pass.Depth->Clear = 0;
  pass.Depth->Begin = Load::Discard;
  pass.Depth->End = Store::Discard;
  Require(commands->BeginRenderPass(pass));
  Require(commands->EndRenderPass());
  pass.Depth->Begin = Load::Clear;
  pass.Depth->End = Store::Store;
  Require(commands->BeginRenderPass(pass));
  Require(commands->EndRenderPass());
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "a depth-only pass supports discard and reverse-Z clear/store");
}

void PassBoundaries(Device &device, const char *shaderPath) {
  const auto pipeline = Require(device.CreateComputePipeline(Spirv(shaderPath)));
  constexpr size_t words = 64;
  constexpr size_t bytes = words * sizeof(uint32_t);
  const auto usage = BufferUse::Storage | BufferUse::CopySource | BufferUse::CopyDestination;
  const auto input = Require(device.CreateBuffer({.Bytes = bytes, .Use = usage}));
  const auto output = Require(device.CreateBuffer({.Bytes = bytes, .Use = usage}));
  const auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes}));
  std::memset(mapped.data(), 0, bytes);
  Require(device.Unmap(*upload));
  const auto pixels = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const auto textureUsage =
      TextureUse::ColourAttachment | TextureUse::CopySource | TextureUse::CopyDestination;
  const auto image = Require(device.CreateTexture(
      {.Width = 1, .Height = 1, .Texels = Format::Rgba8Unorm, .Use = textureUsage}));
  const auto destination = Require(device.CreateTexture(
      {.Width = 1, .Height = 1, .Texels = Format::Rgba8Unorm, .Use = textureUsage}));
  const std::array<uint32_t, words> parameters{};
  const std::array bindings{
      Storage(0, 0, *input),
      Storage(1, 0, *output),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(parameters))}}};
  const std::array targets{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  auto commands = Require(device.Begin());
  Require(commands->Copy(*upload, *input, {.Bytes = bytes}));
  Require(commands->Dispatch(*pipeline, bindings, {}));
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
  Invalid(commands->Dispatch(*pipeline, bindings, {}),
          "valid compute cannot run inside a render pass");
  Invalid(commands->Copy(*output, *input, {.Bytes = bytes}),
          "buffer copies cannot terminate a pass");
  const TextureTransfer transfer{.RowBytes = kRowBytes, .Region = {.Width = 1, .Height = 1}};
  Invalid(commands->Copy(*upload, *image, transfer), "texture uploads cannot terminate a pass");
  Invalid(commands->Copy(*image, *pixels, transfer), "texture readbacks cannot terminate a pass");
  Invalid(
      commands->Copy(*image, {.Width = 1, .Height = 1}, *destination, {.Width = 1, .Height = 1}),
      "texture copies cannot terminate a pass");
  Require(commands->EndRenderPass());
  Require(commands->Dispatch(*pipeline, bindings, {}));
  Require(commands->Copy(*output, *pixels, {.Bytes = bytes}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "compute/render/compute/transfer transitions complete after rejected operations");
  const auto result = Require(device.MapRead(*pixels, {.Bytes = bytes}));
  std::array<uint32_t, words> values{};
  std::memcpy(values.data(), result.data(), bytes);
  for (const auto value : values) {
    CHECK(value == words, "the real compute shader remains usable after the render pass");
  }
  Require(device.Unmap(*pixels));
}

}

int main(int argc, char *const *argv) {
  if (argc != 2) { return 1; }
  auto device = Require(CreateDevice());
  Clears(*device);
  InvalidAttachments(*device);
  DepthActions(*device);
  PassBoundaries(*device, argv[1]);
  return Report();
}
