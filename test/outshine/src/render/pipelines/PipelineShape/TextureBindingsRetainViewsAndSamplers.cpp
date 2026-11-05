#include "Device.h"
#include "Check.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 100;
constexpr uint32_t kHeight = 50;
constexpr auto kWait = std::chrono::seconds(5);

void DrawPixel(Commands &commands,
               const GraphicsPipeline &pipeline,
               const Texture &source,
               const Sampler &sampler,
               uint32_t pixel) {
  const std::array<float, 4> tint{1, 1, 1, 1};
  const std::array bindings{
      Binding{.Set = 2,
              .Slot = 0,
              .Value =
                  SampledTexture{.Image = {.Image = &source, .FirstMip = 1}, .Sampling = &sampler}},
      Binding{.Set = 2, .Slot = 1, .Value = Uniform{.Bytes = std::as_bytes(std::span(tint))}}};
  const uint32_t x = pixel % kWidth;
  const uint32_t y = pixel / kWidth;
  Require(commands.Draw({.State = {.Pipeline = &pipeline,
                                   .Bindings = bindings,
                                   .Vertices = {},
                                   .Area = {.X = static_cast<float>(x),
                                            .Y = static_cast<float>(y),
                                            .Width = 1,
                                            .Height = 1},
                                   .Clip = {.X = x, .Y = y, .Width = 1, .Height = 1}},
                         .Range = {.Count = 3}}));
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto vertex = Shader("fullscreen.vert.spv");
  const auto fragment = Spirv(OUTSHINE_TEXTURE_BINDING_SHADER);
  const std::array colours{ColourTarget{.Texels = Format::Rgba8Unorm, .Rgb = {}, .Alpha = {}}};
  auto pipeline = Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                          .Fragment = ShaderCode{.Spirv = fragment},
                                                          .Streams = {},
                                                          .Attributes = {},
                                                          .Colours = colours,
                                                          .Depth = {},
                                                          .Culling = Cull::None,
                                                          .Label = {}}));
  auto commands = Require(device->Begin());
  std::array<std::unique_ptr<Texture>, 3> sources;
  std::array<std::unique_ptr<Sampler>, 3> samplers;
  for (size_t at = 0; at < sources.size(); ++at) {
    sources[at] =
        Require(device->CreateTexture({.Width = 2,
                                       .Height = 2,
                                       .Mips = 2,
                                       .Use = TextureUse::Sampled | TextureUse::ColourAttachment}));
    samplers[at] =
        Require(device->CreateSampler({.U = at == 0 ? Address::Repeat : Address::ClampToEdge}));
    ColourAttachment attachment{.Image = {.Image = sources[at].get(), .FirstMip = 1},
                                .Begin = Load::Clear,
                                .Clear = {0, 0, 0, 1}};
    attachment.Clear[at] = 1;
    Require(commands->BeginRenderPass({.Colours = std::span(&attachment, 1), .Depth = {}}));
    Require(commands->EndRenderPass());
  }
  const auto target = Require(
      device->CreateTexture({.Width = kWidth,
                             .Height = kHeight,
                             .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  const std::array attachments{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = attachments, .Depth = {}}));
  for (uint32_t at = 0; at < kWidth * kHeight; ++at) {
    DrawPixel(*commands, *pipeline, *sources[at % 3], *samplers[at % 3], at);
  }
  Require(commands->EndRenderPass());
  sources = {};
  samplers = {};
  pipeline.reset();
  CHECK(Require(device->Wait(Require(device->Submit(std::move(commands))), kWait)) ==
            CompletionStatus::Ready,
        "material views survive caller destruction through GPU completion");
  auto readback =
      Require(ReadbackTransfer::Start(*device, *target, {.Width = kWidth, .Height = kHeight}));
  const auto pixels = Require(readback.Wait(*device, kWait));
  CHECK(pixels.has_value(), "completed descriptor-backed draws are readable");
  if (!pixels) { return Report(); }
  uint32_t wrong = 0;
  for (uint32_t at = 0; at < kWidth * kHeight; ++at) {
    for (uint32_t channel = 0; channel < 4; ++channel) {
      const uint8_t expected = channel == 3 || channel == at % 3 ? 255u : 0u;
      const uint64_t offset =
          uint64_t{at / kWidth} * readback.RowBytes() + uint64_t{at % kWidth} * 4 + channel;
      wrong += std::to_integer<uint8_t>((*pixels)[offset]) != expected ? 1u : 0u;
    }
  }
  CHECK(wrong == 0, "all 5000 bindings retain the selected mip and material colour");
  return Report();
}
