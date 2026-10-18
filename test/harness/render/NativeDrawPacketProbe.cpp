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
#include <print>
#include <span>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;
constexpr uint32_t kMiddleDraws = 32769;
constexpr uint32_t kRejectedAfter = 16383;
constexpr uint32_t kRowBytes = 128;
constexpr uint64_t kImageBytes = uint64_t{kRowBytes} * kHeight;
constexpr uint64_t kReadbackBytes = kImageBytes * 3;
constexpr float kNear = .75f;
constexpr float kFar = .25f;
constexpr std::array<float, 4> kClear{0, 0, 1, 1};
constexpr std::array<float, 4> kFirstRed{.5f, 0.f, .5f, .5f};
constexpr std::array<float, 4> kFirstGreen{.25f, .5f, .25f, .5f};
constexpr std::array<float, 4> kSecondRed{0.f, .5f, .5f, .5f};
constexpr std::array<float, 4> kSecondGreen{0.f, .25f, .75f, .5f};

struct alignas(16) Parameters {
  std::array<float, 4> Colour{};
  std::array<float, 4> State{};
};

static_assert(sizeof(Parameters) == 32 && alignof(Parameters) == 16);
static_assert(offsetof(Parameters, State) == 16);

struct Scene {
  std::array<std::unique_ptr<Texture>, 2> Images;
  std::unique_ptr<Texture> Depth;
  std::unique_ptr<Buffer> Pixels;
  std::unique_ptr<Buffer> Indices;
  std::unique_ptr<Buffer> Indirect;
  std::unique_ptr<GraphicsPipeline> Pipeline;
};

struct Shaders {
  std::span<const uint32_t> Vertex;
  std::span<const uint32_t> Fragment;
};

Scene Prepare(Device &device, Shaders shaders) {
  Scene scene{};
  for (auto &image : scene.Images) {
    image = Require(
        device.CreateTexture({.Width = kWidth,
                              .Height = kHeight,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  }
  scene.Depth =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  scene.Pixels = Require(device.CreateBuffer(
      {.Bytes = kReadbackBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  constexpr std::array<uint32_t, 5> indirect{3, 1, 0, 0, 0};
  scene.Indices = Upload(device, *commands, indices, BufferUse::Index);
  scene.Indirect = Upload(device, *commands, indirect, BufferUse::Indirect);
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "dense draw inputs upload before the logical render pass");
  ColourTarget target{};
  target.Texels = Format::Rgba32Float;
  target.Blend = true;
  target.Rgb = {.Source = BlendFactor::SourceAlpha,
                .Destination = BlendFactor::OneMinusSourceAlpha};
  const std::array targets{target, target};
  GraphicsDescription description{};
  description.Vertex.Spirv = shaders.Vertex;
  description.Fragment = ShaderCode{.Spirv = shaders.Fragment};
  description.Colours = targets;
  description.Depth = DepthTarget{};
  description.Culling = Cull::None;
  scene.Pipeline = Require(device.CreateGraphicsPipeline(description));
  return scene;
}

void Begin(Commands &commands, const Scene &scene, Load load) {
  const std::array colours{
      ColourAttachment{.Image = {.Image = scene.Images[0].get()}, .Begin = load, .Clear = kClear},
      ColourAttachment{.Image = {.Image = scene.Images[1].get()}, .Begin = load, .Clear = kClear}};
  Require(commands.BeginRenderPass(
      {.Colours = colours,
       .Depth = DepthAttachment{.Image = {.Image = scene.Depth.get()}, .Begin = load}}));
}

DrawState State(const Scene &scene, std::span<const Binding> bindings) {
  DrawState state{};
  state.Pipeline = scene.Pipeline.get();
  state.Bindings = bindings;
  state.Area.Width = kWidth;
  state.Area.Height = kHeight;
  state.Clip.Width = kWidth;
  state.Clip.Height = kHeight;
  return state;
}

void Record(Commands &commands, const Scene &scene, bool finish = true) {
  Parameters params{.Colour = {1, 0, 0, .5f}, .State = {kNear, 0, 0, 0}};
  const std::array bindings{Binding{.Value = Uniform{std::as_bytes(std::span(&params, 1))}}};
  auto draw = State(scene, bindings);
  const IndexBuffer indices{.Data = scene.Indices.get(), .Range = {.Bytes = 3 * sizeof(uint32_t)}};
  Require(commands.Draw({.State = draw, .Range = {.Count = 3}}));
  params = {.Colour = {0, 0, 1, 0}, .State = {kFar, 0, 0, 0}};
  for (uint32_t at = 0; at < kMiddleDraws; ++at) {
    if (at == kRejectedAfter) {
      const auto rejected =
          commands.DrawIndexed({.State = draw, .Indices = indices, .Range = {.Count = 4}});
      CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
            "an invalid indexed range after an encoded prefix cannot publish a partial draw");
    }
    if (at % 2 == 0) {
      Require(commands.Draw({.State = draw, .Range = {.Count = 3}}));
    } else {
      Require(commands.DrawIndexed({.State = draw, .Indices = indices, .Range = {.Count = 3}}));
    }
  }
  if (!finish) { return; }
  params = {.Colour = {0, 1, 0, .5f}, .State = {1, 0, 0, 0}};
  draw.Clip.X = kWidth / 2;
  draw.Clip.Width = kWidth / 2;
  Require(commands.DrawIndexedIndirect(
      {.State = draw,
       .Indices = indices,
       .Range = {.Data = scene.Indirect.get(), .Count = 1, .Stride = kIndexedIndirectBytes}}));
  Require(commands.EndRenderPass());
}

void Copy(Commands &commands, const Scene &scene) {
  for (size_t at = 0; at < scene.Images.size(); ++at) {
    Require(commands.Copy(*scene.Images[at],
                          *scene.Pixels,
                          {.BufferOffset = at * kImageBytes,
                           .RowBytes = kRowBytes,
                           .Region = {.Width = kWidth, .Height = kHeight}}));
  }
  Require(commands.Copy(*scene.Depth,
                        *scene.Pixels,
                        {.BufferOffset = 2 * kImageBytes,
                         .RowBytes = kRowBytes,
                         .Region = {.Width = kWidth, .Height = kHeight}}));
}

std::array<float, 4> ExpectedPixel(size_t image, bool green, bool drawn) {
  if (!drawn) { return kClear; }
  if (image == 0) { return green ? kFirstGreen : kFirstRed; }
  return green ? kSecondGreen : kSecondRed;
}

void Verify(Device &device, Buffer &pixels, bool drawn) {
  const auto mapped = Require(device.MapRead(pixels, {.Bytes = kReadbackBytes}));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const bool green = x >= kWidth / 2;
      for (size_t image = 0; image < 2; ++image) {
        const auto expected = ExpectedPixel(image, green, drawn);
        std::array<float, 4> value{};
        std::memcpy(value.data(),
                    mapped.data() + image * kImageBytes + size_t{y} * kRowBytes + x * sizeof(value),
                    sizeof(value));
        CHECK(value == expected,
              "all MRT blend pixels survive ordered dense draws or unsubmitted cancellation");
      }
      float depth = 0;
      std::memcpy(&depth,
                  mapped.data() + 2 * kImageBytes + size_t{y} * kRowBytes + x * sizeof(float),
                  sizeof(float));
      const float nearest = green ? 1.f : kNear;
      CHECK(depth == (drawn ? nearest : 0.f),
            "reverse depth survives packet boundaries without another clear");
    }
  }
  Require(device.Unmap(pixels));
}

void Complete(Device &device, Scene scene) {
  auto commands = Require(device.Begin());
  Begin(*commands, scene, Load::Clear);
  Record(*commands, scene);
  Begin(*commands, scene, Load::Load);
  Require(commands->EndRenderPass());
  Copy(*commands, scene);
  for (auto &image : scene.Images) { image.reset(); }
  scene.Depth.reset();
  scene.Indices.reset();
  scene.Indirect.reset();
  scene.Pipeline.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "all dense draw resources survive external-owner release through final completion");
  Verify(device, *scene.Pixels, true);
}

void Cancel(Device &device, Scene scene) {
  auto commands = Require(device.Begin());
  Begin(*commands, scene, Load::Clear);
  Require(commands->EndRenderPass());
  const auto seeded = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(seeded, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "cancellation reference attachments are initialized first");
  commands = Require(device.Begin());
  Begin(*commands, scene, Load::Load);
  Record(*commands, scene, false);
  commands.reset();
  commands = Require(device.Begin());
  Copy(*commands, scene);
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "discarded recording never submits previously encoded packet prefixes");
  Verify(device, *scene.Pixels, false);
}
}

int main(int argc, char *const *argv) {
  if ((argc != 3 && argc != 4) || (argc == 4 && std::string_view(argv[3]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 4}));
  std::println("DEVICE {}", device->Limits().Name);
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  Complete(*device, Prepare(*device, {.Vertex = vertex, .Fragment = fragment}));
  Cancel(*device, Prepare(*device, {.Vertex = vertex, .Fragment = fragment}));
  return Report();
}
