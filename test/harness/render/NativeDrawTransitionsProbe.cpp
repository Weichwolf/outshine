#include "NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 6;
constexpr uint32_t kRowBytes = 256;
constexpr uint64_t kBytes = uint64_t(kRowBytes) * kHeight;
using Pixel = std::array<float, 4>;
constexpr std::array<Pixel, 4> kTexels{{{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}}};
constexpr Pixel kClear{.125f, .25f, .375f, .5f};
constexpr Pixel kTint{.5f, .25f, .75f, .5f};
constexpr float kDepth = .625f;
constexpr float kInstanceAlpha = .25f;
constexpr float kOutputAlpha = .75f;
constexpr uint64_t kArgumentCapacity = 96;

struct Shaders {
  const char *Vertex;
  const char *Fragment;
  const char *Compute;
};
enum class Mode { Direct, Indexed16, Indexed32, Indirect };

struct Parameters {
  Pixel Tint = kTint;
  Pixel Control{0, 0, kDepth, 0};
};

static_assert(sizeof(Parameters) == 32);

std::unique_ptr<Buffer>
Bytes(Device &device, std::span<const std::byte> bytes, UseMask<BufferUse> use) {
  auto buffer = Require(
      device.CreateBuffer({.Bytes = bytes.size(), .Use = use, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*buffer, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*buffer));
  return buffer;
}

struct Inputs {
  std::unique_ptr<Buffer> Vertices, Instances, Shifts, Indices, Arguments;
  std::unique_ptr<Texture> Image;
  std::unique_ptr<Sampler> Sampling;
  IndexBuffer Index;
};

Inputs Prepare(Device &device, Commands &commands, Mode mode, const char *computePath) {
  Inputs input;
  constexpr std::array<std::array<float, 2>, 7> points{
      {{8, 8}, {-1, -1}, {1, -1}, {-1, 1}, {-1, 1}, {1, -1}, {1, 1}}};
  input.Vertices =
      Bytes(device, std::as_bytes(std::span(points)), BufferUse::Vertex | BufferUse::Storage);
  constexpr std::array<std::array<float, 2>, 3> offsets{{{8, 8}, {-8, -8}, {0, 0}}};
  input.Instances = Bytes(device, std::as_bytes(std::span(offsets)), BufferUse::Vertex);
  input.Shifts = Bytes(device, std::as_bytes(std::span(offsets)), BufferUse::Storage);
  constexpr std::array<uint32_t, 11> wide{99, 99, 99, 99, 999, 2, 3, 4, 5, 6, 7};
  constexpr std::array<uint16_t, 11> narrow{99, 99, 99, 99, 999, 2, 3, 4, 5, 6, 7};
  const bool shortIndices = mode == Mode::Indexed16;
  input.Indices = Bytes(device,
                        shortIndices ? std::span<const std::byte>(std::as_bytes(std::span(narrow)))
                                     : std::span<const std::byte>(std::as_bytes(std::span(wide))),
                        BufferUse::Index);
  const uint32_t indexBytes = shortIndices ? 2u : 4u;
  input.Index = {.Data = input.Indices.get(),
                 .Range = {.Offset = uint64_t{4} * indexBytes, .Bytes = uint64_t{7} * indexBytes},
                 .Type = shortIndices ? IndexType::Uint16 : IndexType::Uint32};
  input.Image =
      Require(device.CreateTexture({.Width = 2,
                                    .Height = 2,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto upload = Bytes(device, std::as_bytes(std::span(kTexels)), BufferUse::CopySource);
  Require(commands.Copy(
      *upload, *input.Image, {.RowBytes = 2 * sizeof(Pixel), .Region = {.Width = 2, .Height = 2}}));
  input.Sampling = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  if (mode == Mode::Indirect) {
    input.Arguments = Require(device.CreateBuffer(
        {.Bytes = kArgumentCapacity, .Use = BufferUse::Storage | BufferUse::Indirect}));
    const auto shader = Spirv(computePath);
    const auto pipeline = Require(device.CreateComputePipeline(shader));
    const std::array bindings{Storage(0, 0, *input.Vertices), Storage(0, 1, *input.Arguments)};
    Require(commands.Dispatch(*pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  }
  return input;
}

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const Shaders &shaders, ColourTarget target, Cull cull, Winding front) {
  const auto vertex = Spirv(shaders.Vertex);
  const auto fragment = Spirv(shaders.Fragment);
  const std::array streams{VertexStream{.Slot = 0, .Stride = 8},
                           VertexStream{.Slot = 1, .Stride = 8, .Rate = VertexRate::Instance}};
  const std::array attributes{
      VertexAttribute{.Location = 0, .Stream = 0, .Format = VertexFormat::Float2},
      VertexAttribute{.Location = 1, .Stream = 1, .Format = VertexFormat::Float2}};
  const std::array targets{target};
  auto pipeline = Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                         .Fragment = ShaderCode{.Spirv = fragment},
                                                         .Streams = streams,
                                                         .Attributes = attributes,
                                                         .Colours = targets,
                                                         .Depth = DepthTarget{},
                                                         .Culling = cull,
                                                         .Front = front,
                                                         .Label = {}}));
  const auto &bindings = pipeline->Layout().Bindings;
  CHECK(bindings.size() == 3 && bindings[0].Resource.Set == 0 && bindings[0].Resource.Slot == 0 &&
            bindings[0].Resource.MinimumBytes == sizeof(Parameters) &&
            static_cast<uint32_t>(bindings[0].Stages) == 3 && bindings[1].Resource.Set == 2 &&
            bindings[1].Resource.Kind == BindingKind::SampledTexture &&
            static_cast<uint32_t>(bindings[1].Stages) == 2 && bindings[2].Resource.Set == 3 &&
            bindings[2].Resource.Kind == BindingKind::StorageBuffer &&
            static_cast<uint32_t>(bindings[2].Stages) == 1,
        "graphics reflection merges shared uniforms and retains actual per-stage image/storage "
        "visibility");
  return pipeline;
}

void Invalid(Result<void> result, const char *reason) {
  CHECK(!result && result.error().Kind == ErrorKind::Invalid, reason);
}

void Rejections(Commands &commands, DrawState state, Texture &attachment) {
  auto bad = state;
  bad.Area.Width = std::numeric_limits<float>::quiet_NaN();
  Invalid(commands.Draw({.State = bad, .Range = {.Count = 6}}), "nonfinite viewport is rejected");
  bad = state;
  bad.Clip.X = std::numeric_limits<uint32_t>::max();
  Invalid(commands.Draw({.State = bad, .Range = {.Count = 6}}), "overflowing scissor is rejected");
  Invalid(commands.Draw({.State = state, .Range = {.First = 2, .Count = 6, .FirstInstance = 2}}),
          "direct draw validates the last fetched vertex against its bound byte range");
  Invalid(commands.Draw({.State = state, .Range = {.Count = 3, .FirstInstance = 3}}),
          "instance-rate ranges include FirstInstance");
  auto feedback = std::vector<Binding>(state.Bindings.begin(), state.Bindings.end());
  std::get<SampledTexture>(feedback[1].Value).Image.Image = &attachment;
  bad = state;
  bad.Bindings = feedback;
  Invalid(commands.Draw({.State = bad, .Range = {.Count = 3}}),
          "attachment sampling feedback is rejected before barriers");
}

Pixel Expected(uint32_t x, uint32_t y, const ColourTarget &target, bool culled) {
  if (culled || x < 1 || y < 1 || x >= kWidth - 1 || y >= kHeight - 1) { return kClear; }
  const auto texel = kTexels[(y < kHeight / 2 ? 0u : 2u) + (x < kWidth / 2 ? 0u : 1u)];
  Pixel pixel{};
  for (std::size_t channel = 0; channel < pixel.size(); ++channel) {
    pixel[channel] = texel[channel] * kTint[channel];
  }
  pixel[3] += kInstanceAlpha;
  if (target.Blend) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      pixel[channel] = pixel[channel] * kOutputAlpha + kClear[channel] * kInstanceAlpha;
    }
    pixel[3] += kClear[3] * kInstanceAlpha;
  }
  for (std::size_t channel = 0; channel < pixel.size(); ++channel) {
    if ((static_cast<uint32_t>(target.Write) & (1u << channel)) == 0) {
      pixel[channel] = kClear[channel];
    }
  }
  return pixel;
}

void Case(Device &device,
          const Shaders &shaders,
          Mode mode,
          ColourTarget target,
          Cull cull = Cull::None,
          Winding front = Winding::CounterClockwise,
          bool culled = false) {
  auto commands = Require(device.Begin());
  auto input = Prepare(device, *commands, mode, shaders.Compute);
  auto pipeline = Pipeline(device, shaders, target, cull, front);
  auto colour = Require(device.CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = Format::Rgba32Float,
       .Use = TextureUse::ColourAttachment | TextureUse::CopySource | TextureUse::Sampled}));
  auto depth =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = kBytes * 2, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array attachments{
      ColourAttachment{.Image = {.Image = colour.get()}, .Begin = Load::Clear, .Clear = kClear}};
  Require(commands->BeginRenderPass(
      {.Colours = attachments,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  Parameters params;
  std::array bindings{
      Binding{.Set = 0, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&params, 1))}},
      Binding{.Set = 2,
              .Slot = 0,
              .Value = SampledTexture{.Image = {.Image = input.Image.get()},
                                      .Sampling = input.Sampling.get()}},
      Storage(3, 1, *input.Shifts)};
  std::array vertices{VertexBuffer{.Stream = 0,
                                   .Data = input.Vertices.get(),
                                   .Range = {.Bytes = input.Vertices->Description().Bytes}},
                      VertexBuffer{.Stream = 1,
                                   .Data = input.Instances.get(),
                                   .Range = {.Bytes = input.Instances->Description().Bytes}}};
  const DrawState state{.Pipeline = pipeline.get(),
                        .Bindings = bindings,
                        .Vertices = vertices,
                        .Area = {.Width = kWidth, .Height = kHeight},
                        .Clip = {.X = 1, .Y = 1, .Width = kWidth - 2, .Height = kHeight - 2}};
  Rejections(*commands, state, *colour);
  if (mode == Mode::Direct) {
    Require(
        commands->Draw({.State = state, .Range = {.First = 1, .Count = 6, .FirstInstance = 2}}));
  } else if (mode == Mode::Indirect) {
    Require(commands->DrawIndexedIndirect(
        {.State = state,
         .Indices = input.Index,
         .Range = {.Data = input.Arguments.get(), .Offset = 16, .Count = 2, .Stride = 32}}));
  } else {
    Require(commands->DrawIndexed(
        {.State = state,
         .Indices = input.Index,
         .Range = {.First = 1, .Count = 6, .BaseVertex = -1, .FirstInstance = 2}}));
  }
  params = {.Tint = {}, .Control = {8, 8, 0, 0}};
  vertices = {};
  bindings = {};
  input = {};
  pipeline.reset();
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *colour, *readback, {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  Require(commands->Copy(*depth,
                         *readback,
                         {.BufferOffset = kBytes,
                          .RowBytes = kRowBytes,
                          .Region = {.Width = kWidth, .Height = kHeight}}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "captured draw payloads survive all caller-side mutations and owner releases");
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = kBytes * 2}));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      Pixel pixel{};
      float z = 0;
      std::memcpy(pixel.data(),
                  bytes.data() + std::size_t{y} * kRowBytes + x * sizeof(Pixel),
                  sizeof(Pixel));
      std::memcpy(&z,
                  bytes.data() + kBytes + std::size_t{y} * kRowBytes + x * sizeof(float),
                  sizeof(float));
      const bool drawn = !culled && x >= 1 && y >= 1 && x < kWidth - 1 && y < kHeight - 1;
      CHECK(pixel == Expected(x, y, target, culled),
            "asymmetric top-left texture corners, scissor, blend and channel mask match "
            "independent pixels");
      CHECK(z == (drawn ? kDepth : 0.f),
            "reverse-Z depth follows only the surviving oriented and scissored fragments");
    }
  }
  Require(device.Unmap(*readback));
}
}

int main(int argc, char *const *argv) {
  if ((argc != 4 && argc != 5) || (argc == 5 && std::string_view(argv[4]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 5}));
  const Shaders shaders{.Vertex = argv[1], .Fragment = argv[2], .Compute = argv[3]};
  const ColourTarget target{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}};
  for (const auto mode : {Mode::Direct, Mode::Indexed16, Mode::Indexed32, Mode::Indirect}) {
    Case(*device, shaders, mode, target);
  }
  Case(*device, shaders, Mode::Direct, target, Cull::Back);
  Case(*device, shaders, Mode::Direct, target, Cull::Front, Winding::CounterClockwise, true);
  Case(*device, shaders, Mode::Direct, target, Cull::Back, Winding::Clockwise, true);
  Case(*device, shaders, Mode::Direct, target, Cull::Front, Winding::Clockwise);
  auto blended = target;
  blended.Blend = true;
  blended.Rgb = {.Source = BlendFactor::SourceAlpha,
                 .Destination = BlendFactor::OneMinusSourceAlpha};
  blended.Alpha = {.Source = BlendFactor::One, .Destination = BlendFactor::OneMinusSourceAlpha};
  Case(*device, shaders, Mode::Direct, blended);
  blended.Write = ColourChannel::Red | ColourChannel::Blue;
  Case(*device, shaders, Mode::Direct, blended);
  return Report();
}
