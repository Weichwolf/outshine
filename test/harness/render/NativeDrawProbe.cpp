#include "NativeGpuCheck.h"
#include "Check.h"
#include "Device.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <vector>
#include <variant>
#include <optional>
#include <span>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 8, kHeight = 4, kRowBytes = 256;
constexpr uint64_t kImageBytes = uint64_t{kRowBytes} * kHeight;
constexpr float kRedClear = .125f, kGreenClear = .25f, kDepth = .5078125f, kOccludingDepth = .75f;
constexpr std::array<float, 4> kClear{kRedClear, kGreenClear, .5f, 1};

struct alignas(16) VertexUniform {
  std::array<float, 4> Transform{1, 1, 0, 0};
  std::array<float, 4> Depth{.5f, 0, 0, 0};
};

static_assert(sizeof(VertexUniform) == 32 && alignof(VertexUniform) == 16 &&
              offsetof(VertexUniform, Depth) == 16);

enum class Case { Normal, CullFront, Clockwise, Scissor, DepthReject, Equal, Greater, BlendRed };

std::unique_ptr<Buffer> UploadAs(Device &device,
                                 Commands &commands,
                                 std::span<const std::byte> bytes,
                                 UseMask<BufferUse> use) {
  const auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*upload));
  auto buffer = Require(
      device.CreateBuffer({.Bytes = bytes.size(), .Use = use | BufferUse::CopyDestination}));
  Require(commands.Copy(*upload, *buffer, {.Bytes = bytes.size()}));
  return buffer;
}

std::unique_ptr<Texture> Corners(Device &device, Commands &commands) {
  const auto upload = Require(device.CreateBuffer({.Bytes = uint64_t{kRowBytes} * 2,
                                                   .Use = BufferUse::CopySource,
                                                   .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = uint64_t{kRowBytes} * 2}));
  std::memset(mapped.data(), 0, mapped.size());
  constexpr std::array<uint8_t, 8> top{255, 0, 0, 255, 0, 255, 0, 255};
  constexpr std::array<uint8_t, 8> bottom{0, 0, 255, 255, 255, 255, 0, 255};
  std::memcpy(mapped.data(), top.data(), top.size());
  std::memcpy(mapped.data() + kRowBytes, bottom.data(), bottom.size());
  Require(device.Unmap(*upload));
  auto image =
      Require(device.CreateTexture({.Width = 2,
                                    .Height = 2,
                                    .Texels = Format::Rgba8Unorm,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  Require(
      commands.Copy(*upload, *image, {.RowBytes = kRowBytes, .Region = {.Width = 2, .Height = 2}}));
  return image;
}

std::unique_ptr<GraphicsPipeline> Pipeline(Device &device,
                                           std::span<const uint32_t> vertex,
                                           std::span<const uint32_t> fragment,
                                           Case variant) {
  const std::array streams{VertexStream{.Slot = 0, .Stride = 16},
                           VertexStream{.Slot = 4, .Stride = 8, .Rate = VertexRate::Instance}};
  const std::array attributes{
      VertexAttribute{.Location = 0, .Stream = 0, .Format = VertexFormat::Float2},
      VertexAttribute{.Location = 1, .Stream = 0, .Offset = 8, .Format = VertexFormat::Float2},
      VertexAttribute{.Location = 2, .Stream = 4, .Format = VertexFormat::Float2}};
  ColourTarget target{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}};
  if (variant == Case::BlendRed) {
    target.Blend = true;
    target.Write = ColourChannel::Red;
    target.Rgb = {.Source = BlendFactor::SourceAlpha,
                  .Destination = BlendFactor::OneMinusSourceAlpha};
  }
  const std::array colours{target};
  return Require(device.CreateGraphicsPipeline(
      {.Vertex = {.Spirv = vertex},
       .Fragment = ShaderCode{.Spirv = fragment},
       .Streams = streams,
       .Attributes = attributes,
       .Colours = colours,
       .Depth = DepthTarget{.Compare = variant == Case::Equal ? DepthCompare::GreaterEqual
                                                              : DepthCompare::Greater},
       .Culling = variant == Case::CullFront ? Cull::Front : Cull::Back,
       .Front = variant == Case::Clockwise ? Winding::Clockwise : Winding::CounterClockwise,
       .Label = "native draw contract"}));
}

float DepthClear(Case variant) {
  if (variant == Case::DepthReject) { return kOccludingDepth; }
  if (variant == Case::Equal || variant == Case::Greater) { return kDepth; }
  return 0;
}

bool Covered(Case variant, uint32_t x, uint32_t y) {
  if (variant == Case::CullFront || variant == Case::Clockwise || variant == Case::DepthReject ||
      variant == Case::Greater) {
    return false;
  }
  return variant != Case::Scissor || (x >= 1 && x < kWidth - 1 && y >= 1 && y < kHeight - 1);
}

void CheckPixels(Device &device, Buffer &pixels, Case variant) {
  const auto mapped = Require(device.MapRead(pixels, {.Bytes = kImageBytes * 2}));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const bool drawn = Covered(variant, x, y);
      const bool left = x < kWidth / 2;
      const bool top = y < kHeight / 2;
      constexpr float blueBias = 3.f / 16;
      const std::array<float, 4> surface{left != top ? 0.f : .5f,
                                         !left ? kGreenClear : 0.f,
                                         !top && left ? 1.f + blueBias : blueBias,
                                         .5f};
      std::array<float, 4> expected = drawn ? surface : kClear;
      if (drawn && variant == Case::BlendRed) {
        expected = kClear;
        expected[0] = surface[0] * .5f + kRedClear * .5f;
      }
      std::array<float, 4> actual{};
      std::memcpy(actual.data(),
                  mapped.data() + size_t{y} * kRowBytes + x * sizeof(actual),
                  sizeof(actual));
      CHECK(actual == expected,
            "four asymmetric corners preserve UV, stage bindings, blend and channel masks");
      float depth = 0;
      std::memcpy(&depth,
                  mapped.data() + kImageBytes + size_t{y} * kRowBytes + x * sizeof(float),
                  sizeof(float));
      const float clear = DepthClear(variant);
      CHECK(depth == (drawn ? kDepth : clear),
            "reverse-Z depth, compare, culling and top-left scissor agree");
    }
  }
  Require(device.Unmap(pixels));
}

void InvalidDraws(Commands &commands, const DrawDescription &description, const Texture &target) {
  const auto reject = [&](const DrawDescription &draw) {
    const auto result = commands.Draw(draw);
    CHECK(!result && result.error().Kind == ErrorKind::Invalid,
          "invalid draw inputs are rejected without terminating the pass");
  };
  auto draw = description;
  draw.State.Pipeline = nullptr;
  reject(draw);
  draw = description;
  draw.State.Area.X = std::numeric_limits<float>::quiet_NaN();
  reject(draw);
  draw = description;
  draw.State.Area.Width = kWidth + 1;
  reject(draw);
  draw = description;
  draw.State.Clip.X = std::numeric_limits<uint32_t>::max();
  reject(draw);
  draw = description;
  draw.Range.First = 2;
  reject(draw);
  draw = description;
  draw.Range.FirstInstance = 2;
  reject(draw);
  draw = description;
  draw.State.Bindings = {};
  reject(draw);
  draw = description;
  draw.State.Vertices = {};
  reject(draw);
  const std::array repeated{description.State.Vertices.front(), description.State.Vertices.front()};
  draw.State.Vertices = repeated;
  reject(draw);
  draw = description;
  std::vector<Binding> bindings(description.State.Bindings.begin(),
                                description.State.Bindings.end());
  std::get<SampledTexture>(bindings.back().Value).Image = {.Image = &target};
  draw.State.Bindings = bindings;
  reject(draw);
}

void ReservedImages(Device &device, const Texture &target, Texture &source, Buffer &pixels) {
  auto other = Require(device.Begin());
  const auto targetUse = other->Copy(
      target, pixels, {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}});
  CHECK(!targetUse && targetUse.error().Kind == ErrorKind::Busy,
        "an active render attachment stays reserved by its recording");
  const auto upload = Require(device.CreateBuffer(
      {.Bytes = kRowBytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto sampledUse =
      other->Copy(*upload, source, {.RowBytes = kRowBytes, .Region = {.Width = 1, .Height = 1}});
  CHECK(!sampledUse && sampledUse.error().Kind == ErrorKind::Busy,
        "a sampled draw image stays reserved by its recording");
}

void Draw(Device &device,
          std::span<const uint32_t> vertex,
          std::span<const uint32_t> fragment,
          Case variant) {
  auto pipeline = Pipeline(device, vertex, fragment, variant);
  auto commands = Require(device.Begin());
  constexpr std::array<std::array<float, 4>, 7> vertices{{{0, 0, 0, 0},
                                                          {-1, 1, 0, 0},
                                                          {-1, -1, 0, 1},
                                                          {1, -1, 1, 1},
                                                          {-1, 1, 0, 0},
                                                          {1, -1, 1, 1},
                                                          {1, 1, 1, 0}}};
  constexpr std::array<float, 4> instances{100, 100, 0, 0};
  constexpr std::array<uint32_t, 4> levels{999, 2, 3, 4};
  auto geometry =
      UploadAs(device, *commands, std::as_bytes(std::span(vertices)), BufferUse::Vertex);
  auto offsets =
      UploadAs(device, *commands, std::as_bytes(std::span(instances)), BufferUse::Vertex);
  auto storage = UploadAs(device, *commands, std::as_bytes(std::span(levels)), BufferUse::Storage);
  auto texture = Corners(device, *commands);
  auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  auto target = Require(device.CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = Format::Rgba32Float,
       .Use = TextureUse::ColourAttachment | TextureUse::CopySource | TextureUse::Sampled}));
  auto depth =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  const auto pixels = Require(device.CreateBuffer({.Bytes = kImageBytes * 2,
                                                   .Use = BufferUse::CopyDestination,
                                                   .Placement = Memory::Readback}));
  VertexUniform transform;
  std::array<float, 4> tint{.5f, kGreenClear, 1, .5f};
  const std::array bindings{
      Binding{.Set = 0, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&transform, 1))}},
      Binding{.Set = 1,
              .Slot = 0,
              .Value = StorageBuffer{.Data = storage.get(), .Range = {.Offset = 4, .Bytes = 12}}},
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(tint))}},
      Binding{.Set = 2,
              .Slot = 1,
              .Value =
                  SampledTexture{.Image = {.Image = texture.get()}, .Sampling = sampler.get()}}};
  const std::array streams{
      VertexBuffer{.Stream = 0, .Data = geometry.get(), .Range = {.Bytes = sizeof(vertices)}},
      VertexBuffer{.Stream = 4, .Data = offsets.get(), .Range = {.Bytes = sizeof(instances)}}};
  DrawDescription draw{.State = {.Pipeline = pipeline.get(),
                                 .Bindings = bindings,
                                 .Vertices = streams,
                                 .Area = {.Width = kWidth, .Height = kHeight},
                                 .Clip = {.Width = kWidth, .Height = kHeight}},
                       .Range = {.First = 1, .Count = 6, .FirstInstance = 1}};
  if (variant == Case::Scissor) {
    draw.State.Clip = {.X = 1, .Y = 1, .Width = kWidth - 2, .Height = kHeight - 2};
  }
  const auto outside = commands->Draw(draw);
  CHECK(!outside && outside.error().Kind == ErrorKind::Invalid,
        "a valid draw requires an open pass");
  const std::array colours{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear, .Clear = kClear}};
  const float clear = DepthClear(variant);
  Require(commands->BeginRenderPass({.Colours = colours,
                                     .Depth = DepthAttachment{.Image = {.Image = depth.get()},
                                                              .Begin = Load::Clear,
                                                              .Clear = clear}}));
  if (variant == Case::Normal) { InvalidDraws(*commands, draw, *target); }
  Require(commands->Draw(draw));
  if (variant == Case::Normal) { ReservedImages(device, *target, *texture, *pixels); }
  transform.Transform.fill(0);
  tint.fill(0);
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *target, *pixels, {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  Require(commands->Copy(*depth,
                         *pixels,
                         {.BufferOffset = kImageBytes,
                          .RowBytes = kRowBytes,
                          .Region = {.Width = kWidth, .Height = kHeight}}));
  geometry.reset();
  offsets.reset();
  storage.reset();
  texture.reset();
  sampler.reset();
  target.reset();
  depth.reset();
  pipeline.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "draw resources survive release and uniforms are captured during encoding");
  CheckPixels(device, *pixels, variant);
}

}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 1; }
  auto device = Require(CreateDevice());
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  for (const auto variant : {Case::Normal,
                             Case::CullFront,
                             Case::Clockwise,
                             Case::Scissor,
                             Case::DepthReject,
                             Case::Equal,
                             Case::Greater,
                             Case::BlendRed}) {
    Draw(*device, vertex, fragment, variant);
  }
  return Report();
}
