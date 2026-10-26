#include "NativeGpuCheck.h"
#include "Check.h"
#include "Device.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 8, kHeight = 4, kRowBytes = 256;
constexpr uint64_t kBytes = uint64_t{kRowBytes} * kHeight;
constexpr uint32_t kArgumentBytes = 80, kArgumentOffset = 16, kArgumentStride = 32;
constexpr float kLeftInstance = .25f;

struct alignas(16) Control {
  std::array<int32_t, 4> Ranges;
  std::array<uint32_t, 4> Positions;
};

static_assert(sizeof(Control) == 32 && alignof(Control) == 16 &&
              offsetof(Control, Positions) == 16);

enum class Mode { Direct, Indirect, EmptyDirect, EmptyIndirect };

std::unique_ptr<Buffer> UploadBytes(Device &device,
                                    Commands &commands,
                                    std::span<const std::byte> bytes,
                                    UseMask<BufferUse> use) {
  const auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*upload));
  auto result = Require(
      device.CreateBuffer({.Bytes = bytes.size(), .Use = use | BufferUse::CopyDestination}));
  Require(commands.Copy(*upload, *result, {.Bytes = bytes.size()}));
  return result;
}

template <typename Index>
std::unique_ptr<Buffer> IndexData(Device &device, Commands &commands, int32_t baseVertex) {
  const Index first = baseVertex < 0 ? 2 : 0;
  constexpr Index poison = 100;
  const std::array<Index, 10> values{poison,
                                     poison,
                                     poison,
                                     first,
                                     static_cast<Index>(first + 1),
                                     static_cast<Index>(first + 2),
                                     static_cast<Index>(first + 1),
                                     static_cast<Index>(first + 3),
                                     static_cast<Index>(first + 2),
                                     poison};
  if (baseVertex > 0) {
    auto buffer = Require(device.CreateBuffer(
        {.Bytes = sizeof(values), .Use = BufferUse::Index, .Placement = Memory::Upload}));
    const auto mapped = Require(device.MapWrite(*buffer, {.Bytes = sizeof(values)}));
    std::memcpy(mapped.data(), values.data(), sizeof(values));
    Require(device.Unmap(*buffer));
    return buffer;
  }
  return UploadBytes(device, commands, std::as_bytes(std::span(values)), BufferUse::Index);
}

void Invalid(Result<void> result) {
  CHECK(!result && result.error().Kind == ErrorKind::Invalid,
        "invalid indexed draw metadata is rejected before native encoding");
}

void InvalidDirect(Commands &commands,
                   const IndexedDrawDescription &source,
                   const Buffer &vertices) {
  auto draw = source;
  draw.Indices.Type = static_cast<IndexType>(-1);
  Invalid(commands.DrawIndexed(draw));
  draw = source;
  draw.Indices.Range.Offset = 1;
  Invalid(commands.DrawIndexed(draw));
  draw = source;
  draw.Indices.Range.Bytes -= 1;
  Invalid(commands.DrawIndexed(draw));
  draw = source;
  draw.Indices.Data = &vertices;
  Invalid(commands.DrawIndexed(draw));
  draw = source;
  draw.Range.Count = 9;
  Invalid(commands.DrawIndexed(draw));
  draw = source;
  draw.Range.First = std::numeric_limits<uint32_t>::max();
  Invalid(commands.DrawIndexed(draw));
  draw = source;
  draw.Range.FirstInstance = 2;
  Invalid(commands.DrawIndexed(draw));
  std::thread foreign([&] { Invalid(commands.DrawIndexed(source)); });
  foreign.join();
}

void InvalidIndirect(Commands &commands,
                     const IndirectDrawDescription &source,
                     const Buffer &vertices) {
  auto draw = source;
  draw.Range.Stride = kIndexedIndirectBytes - 4;
  Invalid(commands.DrawIndexedIndirect(draw));
  draw.Range.Stride = kIndexedIndirectBytes + 2;
  Invalid(commands.DrawIndexedIndirect(draw));
  draw = source;
  draw.Range.Offset = 2;
  Invalid(commands.DrawIndexedIndirect(draw));
  draw = source;
  draw.Range.Count = 3;
  Invalid(commands.DrawIndexedIndirect(draw));
  draw = source;
  draw.Range.Offset = std::numeric_limits<uint64_t>::max() - 3;
  Invalid(commands.DrawIndexedIndirect(draw));
  draw = source;
  draw.Range.Count = std::numeric_limits<uint32_t>::max();
  draw.Range.Stride = std::numeric_limits<uint32_t>::max() - 3;
  Invalid(commands.DrawIndexedIndirect(draw));
  draw = source;
  draw.Range.Data = &vertices;
  Invalid(commands.DrawIndexedIndirect(draw));
}

void MappedIndex(Device &device,
                 Commands &commands,
                 Buffer &index,
                 const IndexedDrawDescription &direct,
                 const IndirectDrawDescription &indirect,
                 bool deferred) {
  static_cast<void>(Require(device.MapWrite(index, {.Bytes = index.Description().Bytes})));
  const auto rejected =
      deferred ? commands.DrawIndexedIndirect(indirect) : commands.DrawIndexed(direct);
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Busy,
        "mapped index inputs are rejected without ending the render pass");
  Require(device.Unmap(index));
}

void Pixels(Device &device, Buffer &pixels, bool empty) {
  const auto mapped = Require(device.MapRead(pixels, {.Bytes = kBytes}));
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const std::array<float, 4> expected =
          empty ? std::array<float, 4>{0, 0, 0, 1}
                : std::array<float, 4>{(static_cast<float>(x) + .5f) / kWidth,
                                       (static_cast<float>(y) + .5f) / kHeight,
                                       x < kWidth / 2 ? kLeftInstance : .5f,
                                       1};
      std::array<float, 4> actual{};
      std::memcpy(actual.data(),
                  mapped.data() + size_t{y} * kRowBytes + x * sizeof(actual),
                  sizeof(actual));
      CHECK(actual == expected,
            "direct/indirect draws preserve index type, byte/first offsets, signed base vertex and "
            "base instance");
    }
  }
  Require(device.Unmap(pixels));
}

void Render(Device &device,
            const GraphicsPipeline &pipeline,
            const ComputePipeline &producer,
            IndexType type,
            int32_t baseVertex,
            Mode mode) {
  auto commands = Require(device.Begin());
  constexpr std::array<float, 10> positions{100, 100, -1, -1, 0, -1, -1, 1, 0, 1};
  constexpr std::array<float, 6> offsets{100, 100, 0, 0, 1, 0};
  auto vertices =
      UploadBytes(device, *commands, std::as_bytes(std::span(positions)), BufferUse::Vertex);
  auto instances =
      UploadBytes(device, *commands, std::as_bytes(std::span(offsets)), BufferUse::Vertex);
  auto indices = type == IndexType::Uint16 ? IndexData<uint16_t>(device, *commands, baseVertex)
                                           : IndexData<uint32_t>(device, *commands, baseVertex);
  const uint32_t unit = type == IndexType::Uint16 ? 2 : 4;
  const IndexBuffer index{.Data = indices.get(),
                          .Range = {.Offset = uint64_t{unit} * 2, .Bytes = uint64_t{unit} * 8},
                          .Type = type};
  const bool indirect = mode == Mode::Indirect || mode == Mode::EmptyIndirect;
  const bool empty = mode == Mode::EmptyDirect || mode == Mode::EmptyIndirect;
  auto arguments = Require(device.CreateBuffer(
      {.Bytes = kArgumentBytes, .Use = BufferUse::Storage | BufferUse::Indirect}));
  if (indirect) {
    const Control control{.Ranges = {3, 2, 1, baseVertex}, .Positions = {1, 8, 4, 2}};
    const std::array bindings{
        Storage(1, 0, *arguments),
        Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&control, 1))}}};
    Require(commands->Dispatch(producer, bindings, {}));
  }
  const auto target =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  const auto pixels = Require(device.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array streams{
      VertexBuffer{.Stream = 0, .Data = vertices.get(), .Range = {.Bytes = sizeof(positions)}},
      VertexBuffer{.Stream = 1, .Data = instances.get(), .Range = {.Bytes = sizeof(offsets)}}};
  const DrawState state{.Pipeline = &pipeline,
                        .Bindings = {},
                        .Vertices = streams,
                        .Area = {.Width = kWidth, .Height = kHeight},
                        .Clip = {.Width = kWidth, .Height = kHeight}};
  const IndexedDrawDescription direct{.State = state,
                                      .Indices = index,
                                      .Range = {.First = 1,
                                                .Count = empty ? 0u : 6u,
                                                .Instances = 2,
                                                .BaseVertex = baseVertex,
                                                .FirstInstance = 1}};
  const IndirectDrawDescription deferred{.State = state,
                                         .Indices = index,
                                         .Range = {.Data = arguments.get(),
                                                   .Offset = kArgumentOffset,
                                                   .Count = empty ? 0u : 2u,
                                                   .Stride = kArgumentStride}};
  if (indirect) {
    Invalid(commands->DrawIndexedIndirect(deferred));
  } else {
    Invalid(commands->DrawIndexed(direct));
  }
  const std::array colours{ColourAttachment{
      .Image = {.Image = target.get()}, .Begin = Load::Clear, .Clear = {0, 0, 0, 1}}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  if (type == IndexType::Uint16 && baseVertex < 0 && !empty) {
    if (indirect) {
      InvalidIndirect(*commands, deferred, *vertices);
    } else {
      InvalidDirect(*commands, direct, *vertices);
    }
  }
  if (baseVertex > 0) { MappedIndex(device, *commands, *indices, direct, deferred, indirect); }
  Require(indirect ? commands->DrawIndexedIndirect(deferred) : commands->DrawIndexed(direct));
  if (baseVertex > 0) {
    const auto busy = device.MapWrite(*indices, {.Bytes = indices->Description().Bytes});
    CHECK(!busy && busy.error().Kind == ErrorKind::Busy,
          "recorded index inputs cannot be rewritten");
  }
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *target, *pixels, {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  if (baseVertex < 0) { indices.reset(); }
  vertices.reset();
  instances.reset();
  arguments.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "GPU-generated indirect and indexed draws retain buffers through submission/completion");
  if (indices) {
    static_cast<void>(Require(device.MapWrite(*indices, {.Bytes = indices->Description().Bytes})));
    Require(device.Unmap(*indices));
  }
  Pixels(device, *pixels, empty);
}

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertexPath, const char *fragmentPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  const std::array streams{VertexStream{.Slot = 0, .Stride = 8},
                           VertexStream{.Slot = 1, .Stride = 8, .Rate = VertexRate::Instance}};
  const std::array attributes{
      VertexAttribute{.Location = 0, .Stream = 0, .Format = VertexFormat::Float2},
      VertexAttribute{.Location = 1, .Stream = 1, .Format = VertexFormat::Float2}};
  const std::array colours{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  return Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                .Fragment = ShaderCode{.Spirv = fragment},
                                                .Streams = streams,
                                                .Attributes = attributes,
                                                .Colours = colours,
                                                .Depth = std::nullopt,
                                                .Label = "indexed/indirect comparison"}));
}

}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 1; }
  auto device = Require(CreateDevice());
  const auto pipeline = Pipeline(*device, argv[1], argv[2]);
  const auto producer = Require(device->CreateComputePipeline(Spirv(argv[3])));
  for (const auto type : {IndexType::Uint16, IndexType::Uint32}) {
    for (const auto base : {-1, 1}) {
      for (const auto mode :
           {Mode::Direct, Mode::Indirect, Mode::EmptyDirect, Mode::EmptyIndirect}) {
        Render(*device, *pipeline, *producer, type, base, mode);
      }
    }
  }
  return Report();
}
