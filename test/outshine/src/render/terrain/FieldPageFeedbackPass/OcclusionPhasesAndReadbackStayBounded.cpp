#include "FieldPageFeedbackPass.h"
#include "FieldPageFeedback.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct alignas(16) Request {
  int32_t X, Y, Mip, Valid;
};

static_assert(sizeof(Request) == 16 && alignof(Request) == 16 && offsetof(Request, X) == 0 &&
              offsetof(Request, Y) == 4 && offsetof(Request, Mip) == 8 &&
              offsetof(Request, Valid) == 12);

std::vector<Request> Inputs() {
  std::vector<Request> keys(size_t{13} * 9);
  for (int32_t y = 0; y < 9; ++y) {
    for (int32_t x = 0; x < 13; ++x) {
      keys[size_t{static_cast<uint32_t>(y)} * 13 + static_cast<uint32_t>(x)] = {
          .X = std::numeric_limits<int32_t>::max() - x,
          .Y = std::numeric_limits<int32_t>::min() + y,
          .Mip = (x + y) % 32,
          .Valid = (x + y) % 5 != 0 ? 1 : 0};
    }
  }
  return keys;
}

void Raster(Device &device, const Texture &image, const std::vector<Request> &keys) {
  const auto vertex = Shader("fullscreen.vert.spv");
  const auto fragment = Spirv(OUTSHINE_FIELD_FEEDBACK_SHADER);
  const auto depth = Require(device.CreateTexture({.Width = 13,
                                                   .Height = 9,
                                                   .Texels = Format::Depth32Float,
                                                   .Use = TextureUse::DepthAttachment}));
  const auto opaque = Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                             .Fragment = std::nullopt,
                                                             .Streams = {},
                                                             .Attributes = {},
                                                             .Colours = {},
                                                             .Depth = DepthTarget{},
                                                             .Culling = Cull::None,
                                                             .Label = "feedback occluder"}));
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  const auto terrain =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                             .Fragment = ShaderCode{.Spirv = fragment},
                                             .Streams = {},
                                             .Attributes = {},
                                             .Colours = targets,
                                             .Depth = DepthTarget{},
                                             .Culling = Cull::None,
                                             .Label = "visible feedback"}));
  auto commands = Require(device.Begin());
  std::vector<uint32_t> words(keys.size() * 4);
  std::memcpy(words.data(), keys.data(), keys.size() * sizeof(Request));
  const auto input = Upload(device, *commands, words);
  Require(commands->BeginRenderPass(
      {.Colours = {},
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  Require(commands->Draw(
      {.State = {.Pipeline = opaque.get(),
                 .Bindings = {},
                 .Vertices = {},
                 .Area = {.Width = 13, .Height = 9, .MinDepth = 0.75f, .MaxDepth = 0.75f},
                 .Clip = {.Width = 5, .Height = 9}},
       .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  const std::array colours{ColourAttachment{.Image = {.Image = &image}, .Begin = Load::Clear}};
  const std::array bindings{Storage(0, 0, *input)};
  Require(commands->BeginRenderPass(
      {.Colours = colours,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Load}}));
  Require(commands->Draw(
      {.State = {.Pipeline = terrain.get(),
                 .Bindings = bindings,
                 .Vertices = {},
                 .Area = {.Width = 13, .Height = 9, .MinDepth = 0.5f, .MaxDepth = 0.5f},
                 .Clip = {.Width = 13, .Height = 9}},
       .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  (void)Require(device.Submit(std::move(commands)));
}

ReadbackTransfer Take(FieldPageFeedbackPass &feedback) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    auto ready = feedback.Take();
    if (!ready) {
      CHECK(false, "native feedback poll succeeds");
      Failed();
    }
    if (*ready) { return std::move(**ready); }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(false, "native feedback readback completes within its test deadline");
  Failed();
}

void Probe(Device &device) {
  const auto keys = Inputs();
  const auto image =
      Require(device.CreateTexture({.Width = 13,
                                    .Height = 9,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::Sampled}));
  Raster(device, *image, keys);
  FieldPageFeedbackPass feedback;
  constexpr uint64_t bytes = uint64_t{4} * 3 * sizeof(FieldPageFeedbackPixel);
  CHECK(!feedback.Configure(device, *image, {.Stride = 4, .StorageBytesMost = bytes * 3 - 1}),
        "all output/pending/consumer readback reservations are admitted before allocation");
  Require(feedback.Configure(device, *image, {.Stride = 4, .StorageBytesMost = bytes * 3}));
  CHECK(feedback.Samples() == 12 && !Require(feedback.BeginReadback()),
        "an unrecorded output cannot be read back");
  std::array<bool, size_t{13} * 9> seen{};
  for (uint32_t phase = 0; phase < 16; ++phase) {
    auto commands = Require(device.Begin());
    CHECK(Require(feedback.Encode(*commands, phase)),
          "an idle feedback phase records exactly once");
    (void)Require(device.Submit(std::move(commands)));
    CHECK(Require(feedback.BeginReadback()) && !Require(feedback.BeginReadback()),
          "one pending readback owns its slot until taken");
    auto omitted = Require(device.Begin());
    CHECK(!Require(feedback.Encode(*omitted, phase)),
          "unconsumed feedback prevents an extra dispatch");
    omitted.reset();
    auto holder = Take(feedback);
    const auto mapped = Require(holder.Poll(device));
    CHECK(mapped, "the transferred holder retains its ready mapped bytes");
    if (!mapped) { Failed(); }
    for (uint32_t i = 0; i < 12; ++i) {
      const uint32_t x = std::min((i % 4) * 4 + phase % 4, 12u);
      const uint32_t y = std::min((i / 4) * 4 + phase / 4, 8u);
      const size_t at = size_t{y} * 13 + x;
      seen[at] = true;
      FieldPageFeedbackPixel pixel;
      std::memcpy(&pixel, mapped->data() + i * sizeof(pixel), sizeof(pixel));
      const auto decoded = Require(DecodeFieldPageFeedback(pixel));
      const auto expected =
          x < 5 || keys[at].Valid == 0
              ? std::optional<FieldPageAddress>{}
              : std::optional{FieldPageAddress{
                    .X = keys[at].X, .Y = keys[at].Y, .Mip = static_cast<uint32_t>(keys[at].Mip)}};
      CHECK(decoded == expected,
            "only visible sampled fragments request their full integer page key");
    }
  }
  CHECK(std::ranges::all_of(seen, [](bool value) { return value; }),
        "sampling phases cover every pixel, including partial right/bottom grid cells");
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Probe(*device);
  return outshine::Test::Report();
}
