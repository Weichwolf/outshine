#include "Device.h"
#include "Readback.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace N = outshine::Render::Native;
constexpr uint32_t kWidth = 1280, kHeight = 720;
constexpr size_t kTargetTriangle = 195;
constexpr int kPixelX = 442, kPixelY = 239;
using Four = std::array<double, 4>;
constexpr size_t kViewElements = 56;
constexpr size_t kPlacementElements = 36;
constexpr size_t kMaterialBytes = 384;
constexpr size_t kLightBytes = 1344;
constexpr size_t kIndexCount = 1038;
constexpr size_t kCurrentShiftOffset = 48;
constexpr size_t kPreviousShiftOffset = 52;
constexpr auto kWaitBudget = std::chrono::seconds(30);
constexpr auto kMaximumShaderBytes = std::streamoff{16} * 1024 * 1024;
constexpr unsigned kHalfExponentMask = 31;
constexpr unsigned kHalfMantissaMask = 1023;
constexpr unsigned kHalfSignMask = 32768;
constexpr int kHalfFractionBits = 10;
constexpr int kHalfSubnormalExponent = -24;
constexpr int kHalfMagnitudeShift = 25;
static_assert(std::endian::native == std::endian::little);

[[noreturn]] void Fail(const char *message) {
  std::println(stderr, "{}", message);
  std::fflush(stderr);
  std::fflush(stdout);
  std::_Exit(2);
}

template <typename T> T Require(N::Result<T> result) {
  if (!result) { Fail(result.error().Message.c_str()); }
  return std::move(*result);
}

void Require(N::Result<void> result) {
  if (!result) { Fail(result.error().Message.c_str()); }
}

template <typename T>
std::vector<T> Read(const std::filesystem::path &path, std::streamoff maximumBytes = 65536) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = file ? static_cast<std::streamoff>(file.tellg()) : -1;
  if (bytes <= 0 || bytes > maximumBytes || bytes % sizeof(T) != 0) {
    Fail("fixture files must be nonempty, bounded and element aligned");
  }
  std::vector<T> result(static_cast<size_t>(bytes) / sizeof(T));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(result.data()), bytes);
  if (!file) { Fail("fixture input is incomplete"); }
  return result;
}

struct Input {
  std::array<std::vector<std::byte>, 4> Streams;
  std::vector<uint32_t> Indices;
  std::vector<float> View, Placement;
  std::vector<std::byte> Material, Light;
};

Input Load(const std::filesystem::path &path) {
  Input input;
  for (size_t i = 0; i < input.Streams.size(); ++i) {
    input.Streams[i] = Read<std::byte>(path / ("stream-" + std::to_string(i) + ".bin"));
  }
  input.Indices = Read<uint32_t>(path / "indices.bin");
  input.View = Read<float>(path / "view.bin");
  input.Placement = Read<float>(path / "placement.bin");
  input.Material = Read<std::byte>(path / "material.bin");
  input.Light = Read<std::byte>(path / "light.bin");
  const size_t vertices = input.Streams[0].size() / 12;
  if (input.View.size() != kViewElements || input.Placement.size() != kPlacementElements ||
      input.Material.size() != kMaterialBytes || input.Light.size() != kLightBytes ||
      input.Streams[0].size() != vertices * 12 || input.Streams[1].size() != vertices * 8 ||
      input.Streams[2].size() != vertices * 8 || input.Streams[3].size() != vertices * 4 ||
      input.Indices.size() != kIndexCount ||
      !std::ranges::all_of(input.Indices, [vertices](uint32_t v) { return v < vertices; })) {
    Fail("fixture sizes or indices violate the preserved shader input contract");
  }
  return input;
}

Four Transform(std::span<const float> matrix, const Four &point) {
  Four result{};
  for (size_t row = 0; row < 4; ++row) {
    for (size_t col = 0; col < 4; ++col) { result[row] += matrix[col * 4 + row] * point[col]; }
  }
  return result;
}

std::array<double, 2> Expected(const Input &input) {
  std::array<Four, 3> clip{};
  std::array<Four, 3> previous{};
  std::array<std::array<double, 2>, 3> screen{};
  for (size_t i = 0; i < 3; ++i) {
    std::array<float, 3> position{};
    const size_t index = input.Indices[kTargetTriangle * 3 + i];
    std::memcpy(position.data(), input.Streams[0].data() + index * 12, 12);
    const Four point{position[0], position[1], position[2], 1};
    auto current = Transform(std::span(input.Placement).first(16), point);
    auto before = Transform(std::span(input.Placement).subspan(16, 16), point);
    for (size_t axis = 0; axis < 3; ++axis) {
      current[axis] += input.View[kCurrentShiftOffset + axis];
      before[axis] += input.View[kPreviousShiftOffset + axis];
    }
    clip[i] = Transform(std::span(input.View).first(16), current);
    previous[i] = Transform(std::span(input.View).subspan(16, 16), before);
    if (!(clip[i][3] > 0)) { Fail("the reference triangle must be in front of the camera"); }
    screen[i] = {(clip[i][0] / clip[i][3] * 0.5 + 0.5) * kWidth,
                 (0.5 - clip[i][1] / clip[i][3] * 0.5) * kHeight};
  }
  const auto cross = [](auto a, auto b) { return a[0] * b[1] - a[1] * b[0]; };
  const std::array ab{screen[1][0] - screen[0][0], screen[1][1] - screen[0][1]};
  const std::array ac{screen[2][0] - screen[0][0], screen[2][1] - screen[0][1]};
  const std::array ap{kPixelX + 0.5 - screen[0][0], kPixelY + 0.5 - screen[0][1]};
  const double area = cross(ab, ac);
  if (!std::isfinite(area) || std::abs(area) < 1) { Fail("the reference triangle is degenerate"); }
  const double u = cross(ap, ac) / area;
  const double v = cross(ab, ap) / area;
  const std::array weights{1 - u - v, u, v};
  if (!std::ranges::all_of(weights, [](double x) { return x > 0; })) {
    Fail("the checked pixel must be strictly inside the reference triangle");
  }
  Four before{};
  for (size_t i = 0; i < 3; ++i) {
    for (size_t axis = 0; axis < 4; ++axis) {
      before[axis] += weights[i] * previous[i][axis] / clip[i][3];
    }
  }
  const std::array result{(kPixelX + 0.5) * 2 / kWidth - 1 - before[0] / before[3],
                          1 - (kPixelY + 0.5) * 2 / kHeight - before[1] / before[3]};
  if (!std::ranges::all_of(result, [](double x) { return std::isfinite(x); })) {
    Fail("the independent projected motion must be finite");
  }
  return result;
}

void Finish(N::Device &device, std::unique_ptr<N::Commands> commands) {
  const auto done = Require(device.Submit(std::move(commands)));
  if (Require(device.Wait(done, kWaitBudget)) != N::CompletionStatus::Ready) {
    Fail("native submission did not complete");
  }
}

std::unique_ptr<N::Buffer>
Upload(N::Device &device, std::span<const std::byte> bytes, N::BufferUse usage) {
  auto source = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = N::BufferUse::CopySource, .Placement = N::Memory::Upload}));
  auto target = Require(
      device.CreateBuffer({.Bytes = bytes.size(), .Use = usage | N::BufferUse::CopyDestination}));
  auto mapped = Require(device.MapWrite(*source, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*source));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*source, *target, {.Bytes = bytes.size()}));
  Finish(device, std::move(commands));
  return target;
}

std::unique_ptr<N::Texture> Dummy(N::Device &device, bool white) {
  std::array<uint8_t, 256> bytes{};
  bytes.fill(white ? std::numeric_limits<uint8_t>::max() : 0);
  auto texture = Require(
      device.CreateTexture({.Width = 8,
                            .Height = 8,
                            .Texels = N::Format::Rgba8Unorm,
                            .Use = N::TextureUse::Sampled | N::TextureUse::CopyDestination}));
  auto source = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = N::BufferUse::CopySource, .Placement = N::Memory::Upload}));
  auto mapped = Require(device.MapWrite(*source, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*source));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*source, *texture, {.Region = {.Width = 8, .Height = 8}}));
  Finish(device, std::move(commands));
  return texture;
}

struct Fixture {
  std::unique_ptr<N::Device> Device;
  std::array<std::unique_ptr<N::Buffer>, 4> Streams;
  std::unique_ptr<N::Buffer> Placements, Indices, Indirect;
  std::unique_ptr<N::Texture> White, Black, Hdr, Velocity, Depth;
  std::unique_ptr<N::Sampler> Sampler;
  std::unique_ptr<N::GraphicsPipeline> Pipeline;
  std::vector<N::Binding> Bindings;
  std::array<N::VertexBuffer, 5> Vertices;
};

std::unique_ptr<N::GraphicsPipeline> PreparePipeline(N::Device &device,
                                                     std::span<const uint32_t> vertex,
                                                     std::span<const uint32_t> fragment) {
  const std::array streams{N::VertexStream{.Slot = 0, .Stride = 12},
                           N::VertexStream{.Slot = 1, .Stride = 8},
                           N::VertexStream{.Slot = 2, .Stride = 8},
                           N::VertexStream{.Slot = 3, .Stride = 4},
                           N::VertexStream{.Slot = 4, .Stride = 12}};
  const std::array attributes{
      N::VertexAttribute{.Location = 0, .Stream = 0, .Format = N::VertexFormat::Float3},
      N::VertexAttribute{.Location = 1, .Stream = 1, .Format = N::VertexFormat::Float2},
      N::VertexAttribute{.Location = 6, .Stream = 2, .Format = N::VertexFormat::Float2},
      N::VertexAttribute{.Location = 3, .Stream = 3, .Format = N::VertexFormat::Uint32},
      N::VertexAttribute{.Location = 5, .Stream = 4, .Format = N::VertexFormat::Float3}};
  const std::array colours{
      N::ColourTarget{.Texels = N::Format::Rgba16Float, .Rgb = {}, .Alpha = {}},
      N::ColourTarget{.Texels = N::Format::Rg16Float, .Rgb = {}, .Alpha = {}}};
  return Require(device.CreateGraphicsPipeline(
      {.Vertex = {.Spirv = vertex},
       .Fragment = N::ShaderCode{.Spirv = fragment},
       .Streams = streams,
       .Attributes = attributes,
       .Colours = colours,
       .Depth = N::DepthTarget{.Compare = N::DepthCompare::GreaterEqual},
       .Culling = N::Cull::None,
       .ClampDepth = true,
       .Label = {}}));
}

Fixture
Prepare(const Input &input, std::span<const uint32_t> vertex, std::span<const uint32_t> fragment) {
  Fixture f;
  f.Device = Require(N::CreateDevice({.Validation = true}));
  auto &device = *f.Device;
  std::println("DEVICE {}", device.Limits().Name);
  for (size_t i = 0; i < f.Streams.size(); ++i) {
    f.Streams[i] = Upload(device, input.Streams[i], N::BufferUse::Vertex);
  }
  f.Placements = Upload(device, std::as_bytes(std::span(input.Placement)), N::BufferUse::Storage);
  f.Indices = Upload(device, std::as_bytes(std::span(input.Indices)), N::BufferUse::Index);
  const N::IndexedIndirectArguments args{.IndexCount = static_cast<uint32_t>(input.Indices.size()),
                                         .InstanceCount = 1};
  f.Indirect = Upload(device, std::as_bytes(std::span(&args, size_t{1})), N::BufferUse::Indirect);
  f.White = Dummy(device, true);
  f.Black = Dummy(device, false);
  f.Sampler = Require(device.CreateSampler({}));
  const auto target = [&](N::Format format) {
    return Require(
        device.CreateTexture({.Width = kWidth,
                              .Height = kHeight,
                              .Texels = format,
                              .Use = N::TextureUse::ColourAttachment | N::TextureUse::CopySource}));
  };
  f.Hdr = target(N::Format::Rgba16Float);
  f.Velocity = target(N::Format::Rg16Float);
  f.Depth = Require(device.CreateTexture({.Width = kWidth,
                                          .Height = kHeight,
                                          .Texels = N::Format::Depth32Float,
                                          .Use = N::TextureUse::DepthAttachment}));
  f.Pipeline = PreparePipeline(device, vertex, fragment);
  for (uint32_t slot = 0; slot < 9; ++slot) {
    f.Bindings.push_back(
        {.Set = 2,
         .Slot = slot,
         .Value = N::SampledTexture{.Image = {.Image = slot == 7 ? f.Black.get() : f.White.get()},
                                    .Sampling = f.Sampler.get()}});
  }
  f.Bindings.push_back(
      {.Set = 0,
       .Slot = 0,
       .Value = N::StorageBuffer{.Data = f.Placements.get(),
                                 .Range = {.Bytes = f.Placements->Description().Bytes}}});
  f.Bindings.push_back(
      {.Set = 1, .Slot = 0, .Value = N::Uniform{.Bytes = std::as_bytes(std::span(input.View))}});
  f.Bindings.push_back({.Set = 3, .Slot = 0, .Value = N::Uniform{.Bytes = input.Material}});
  f.Bindings.push_back({.Set = 3, .Slot = 1, .Value = N::Uniform{.Bytes = input.Light}});
  for (uint32_t slot = 0; slot < 5; ++slot) {
    auto *buffer = f.Streams[slot == 4 ? 0 : slot].get();
    f.Vertices[slot] = {
        .Stream = slot, .Data = buffer, .Range = {.Bytes = buffer->Description().Bytes}};
  }
  return f;
}

double Half(uint16_t value) {
  const int exponent = static_cast<int>((value >> kHalfFractionBits) & kHalfExponentMask);
  const int mantissa = static_cast<int>(value & kHalfMantissaMask);
  const bool negative = (value & kHalfSignMask) != 0;
  if (std::cmp_equal(exponent, kHalfExponentMask)) {
    return mantissa != 0
               ? std::numeric_limits<double>::quiet_NaN()
               : std::copysign(std::numeric_limits<double>::infinity(), negative ? -1 : 1);
  }
  const double magnitude = exponent == 0 ? std::ldexp(mantissa, kHalfSubnormalExponent)
                                         : std::ldexp(mantissa + (1 << kHalfFractionBits),
                                                      exponent - kHalfMagnitudeShift);
  return negative ? -magnitude : magnitude;
}

int Inspect(Fixture &f, const std::array<double, 2> &expected, int iterations) {
  const std::array attachments{
      N::ColourAttachment{.Image = {.Image = f.Hdr.get()}, .Begin = N::Load::Clear},
      N::ColourAttachment{.Image = {.Image = f.Velocity.get()}, .Begin = N::Load::Clear}};
  size_t badFrames = 0;
  size_t badComponents = 0;
  double maximum = 0;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    auto commands = Require(f.Device->Begin());
    Require(commands->BeginRenderPass(
        {.Colours = attachments,
         .Depth = N::DepthAttachment{.Image = {.Image = f.Depth.get()}, .Begin = N::Load::Clear}}));
    Require(commands->DrawIndexedIndirect(
        {.State = {.Pipeline = f.Pipeline.get(),
                   .Bindings = f.Bindings,
                   .Vertices = f.Vertices,
                   .Area = {.Width = kWidth, .Height = kHeight},
                   .Clip = {.Width = kWidth, .Height = kHeight}},
         .Indices = {.Data = f.Indices.get(), .Range = {.Bytes = f.Indices->Description().Bytes}},
         .Range = {.Data = f.Indirect.get(), .Count = 1}}));
    Require(commands->EndRenderPass());
    Finish(*f.Device, std::move(commands));
    outshine::Render::Readback read;
    if (read.FromTexture(
            f.Device.get(), f.Velocity.get(), {.WidthPx = kWidth, .HeightPx = kHeight}, 4) !=
        outshine::Render::ReadState::Ready) {
      Fail("native velocity readback failed");
    }
    std::array<uint16_t, 2> half{};
    std::memcpy(half.data(),
                read.Rows() + static_cast<size_t>(kPixelY) * read.RowBytes() +
                    static_cast<size_t>(kPixelX) * sizeof(half),
                4);
    std::array<double, 2> actual{Half(half[0]), Half(half[1])};
    bool bad = false;
    for (size_t axis = 0; axis < 2; ++axis) {
      const double error = std::abs(actual[axis] - expected[axis]);
      const double exponent =
          expected[axis] == 0
              ? -24
              : std::max(-24.0, std::floor(std::log2(std::abs(expected[axis]))) - 10);
      const double bound = 2 * std::exp2(exponent) + 8 * std::numeric_limits<float>::epsilon();
      maximum =
          std::isfinite(error) ? std::max(maximum, error) : std::numeric_limits<double>::infinity();
      if (!std::isfinite(error) || error > bound) {
        ++badComponents;
        bad = true;
      }
    }
    if (bad) {
      if (badFrames < 4) {
        std::println("BAD iteration={} velocity={:.12g},{:.12g} expected={:.12g},{:.12g}",
                     iteration,
                     actual[0],
                     actual[1],
                     expected[0],
                     expected[1]);
      }
      ++badFrames;
    }
  }
  std::println("RESULT frames={} bad_frames={} checks={} bad_components={} max_ndc_error={:.12g}",
               iterations,
               badFrames,
               iterations * 2,
               badComponents,
               maximum);
  return badComponents == 0 ? 0 : 6;
}
}

int main(int argc, char **argv) {
  if (argc != 5) { Fail("expected fixture directory, vertex SPIR-V, fragment SPIR-V, iterations"); }
  char *end = nullptr;
  const long iterations = std::strtol(argv[4], &end, 10);
  if (end == argv[4] || *end != '\0' || iterations < 1 || iterations > 4096) {
    Fail("iterations must be between 1 and 4096");
  }
  const auto input = Load(argv[1]);
  const auto vertex = Read<uint32_t>(argv[2], kMaximumShaderBytes);
  const auto fragment = Read<uint32_t>(argv[3], kMaximumShaderBytes);
  const auto expected = Expected(input);
  auto fixture = Prepare(input, vertex, fragment);
  return Inspect(fixture, expected, static_cast<int>(iterations));
}
