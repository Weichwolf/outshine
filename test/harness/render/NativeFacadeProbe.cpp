#include "NativeGpuCheck.h"
#include "Check.h"
#include "Device.h"
#include "Commands.h"
#include "Resources.h"
#include "Graphics.h"
#include "Bindings.h"
#include "Render.h"
#include "spatial/TangentFrame.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <ios>
#include <print>
#include <span>
#include <string_view>
#include <vector>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 160, kHeight = 96, kRowBytes = kWidth * 16;
constexpr uint64_t kImageBytes = uint64_t{kRowBytes} * kHeight;
constexpr size_t kViewBytes = 224, kMaterialBytes = 384, kPlacementBytes = 144;

struct Vertex {
  std::array<float, 3> Position;
  std::array<float, 2> Uv;
  uint32_t Normal;
};

constexpr size_t kVertexStride = 24, kNormalOffset = 20;
static_assert(sizeof(Vertex) == kVertexStride && offsetof(Vertex, Uv) == 12 &&
              offsetof(Vertex, Normal) == kNormalOffset);
constexpr size_t kVerticesBytes = 6 * sizeof(Vertex);
constexpr size_t kInputBytes = kViewBytes + kMaterialBytes + kPlacementBytes + kVerticesBytes;

std::vector<std::byte> Read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = file ? static_cast<std::streamoff>(file.tellg()) : -1;
  const bool complete = file && std::cmp_equal(bytes, kInputBytes);
  CHECK(complete, "the fixed fixture contains complete view/material/placement/vertex inputs");
  if (!complete) { Failed(); }
  std::vector<std::byte> contents(kInputBytes);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(contents.data()),
            static_cast<std::streamsize>(contents.size()));
  CHECK(file, "the complete raster fixture is readable");
  if (!file) { Failed(); }
  return contents;
}

std::unique_ptr<Buffer> UploadAs(Device &device,
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

void Capture(Device &device,
             const char *vertexPath,
             const char *fragmentPath,
             std::span<const std::byte> input,
             const char *outputPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  const std::array streams{VertexStream{.Slot = 0, .Stride = sizeof(Vertex)}};
  const std::array attributes{
      VertexAttribute{.Location = 0, .Stream = 0, .Format = VertexFormat::Float3},
      VertexAttribute{.Location = 1, .Stream = 0, .Offset = 12, .Format = VertexFormat::Float2},
      VertexAttribute{
          .Location = 3, .Stream = 0, .Offset = kNormalOffset, .Format = VertexFormat::Uint32}};
  std::array<ColourTarget, 3> targets{};
  for (auto &target : targets) { target.Texels = Format::Rgba32Float; }
  const auto pipeline =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                             .Fragment = ShaderCode{.Spirv = fragment},
                                             .Streams = streams,
                                             .Attributes = attributes,
                                             .Colours = targets,
                                             .Depth = std::nullopt,
                                             .Culling = Cull::None,
                                             .Label = "native facade material"}));
  auto commands = Require(device.Begin());
  const auto placement = UploadAs(device,
                                  *commands,
                                  input.subspan(kViewBytes + kMaterialBytes, kPlacementBytes),
                                  BufferUse::Storage);
  const auto geometry = UploadAs(device, *commands, input.last(kVerticesBytes), BufferUse::Vertex);
  const std::array bindings{
      Storage(0, 0, *placement),
      Binding{.Set = 1, .Slot = 0, .Value = Uniform{input.first(kViewBytes)}},
      Binding{.Set = 3, .Slot = 0, .Value = Uniform{input.subspan(kViewBytes, kMaterialBytes)}}};
  const std::array vertices{
      VertexBuffer{.Stream = 0, .Data = geometry.get(), .Range = {.Bytes = kVerticesBytes}}};
  std::array<std::unique_ptr<Texture>, 3> images;
  std::array<ColourAttachment, 3> colours;
  for (size_t index = 0; index < images.size(); ++index) {
    images[index] = Require(
        device.CreateTexture({.Width = kWidth,
                              .Height = kHeight,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    colours[index] = {
        .Image = {.Image = images[index].get()}, .Begin = Load::Clear, .Clear = {-1, -1, -1, -1}};
  }
  const auto download = Require(device.CreateBuffer({.Bytes = kImageBytes * images.size(),
                                                     .Use = BufferUse::CopyDestination,
                                                     .Placement = Memory::Readback}));
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = pipeline.get(),
                                    .Bindings = bindings,
                                    .Vertices = vertices,
                                    .Area = {.Width = kWidth, .Height = kHeight},
                                    .Clip = {.Width = kWidth, .Height = kHeight}},
                          .Range = {.Count = 6}}));
  Require(commands->EndRenderPass());
  for (size_t index = 0; index < images.size(); ++index) {
    Require(commands->Copy(*images[index],
                           *download,
                           {.BufferOffset = index * kImageBytes,
                            .RowBytes = kRowBytes,
                            .Region = {.Width = kWidth, .Height = kHeight}}));
  }
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "the real facade raster completes within the capture timeout");
  const auto mapped = Require(device.MapRead(*download, {.Bytes = kImageBytes * images.size()}));
  std::ofstream file(outputPath, std::ios::binary);
  file.write(reinterpret_cast<const char *>(mapped.data()),
             static_cast<std::streamsize>(mapped.size()));
  CHECK(file, "all three real raster attachments are saved");
  Require(device.Unmap(*download));
}

void Frame() {
  const auto frame = outshine::TangentFrame::At({.LongitudeDeg = 16.37, .LatitudeDeg = 48.2});
  for (const auto &axis :
       {frame.OriginEcef(), frame.EastEcef(), frame.UpEcef(), frame.NorthEcef()}) {
    std::println("{:.17g} {:.17g} {:.17g}", axis[0], axis[1], axis[2]);
  }
}
}

int main(int argc, char *const *argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--frame") {
    Frame();
    return 0;
  }
  if (argc != 5) { return 1; }
  const auto input = Read(argv[3]);
  auto device = Require(CreateDevice({.Validation = true}));
  Capture(*device, argv[1], argv[2], input, argv[4]);
  return Report();
}
