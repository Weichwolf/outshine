#include "GpuSupport.h"
#include "Readback.h"
#include "SpirvFile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <limits>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace Native = outshine::Render::Native;
namespace Calibration = outshine::Diagnostics::Calibration;

namespace {
using Bytes = std::vector<std::byte>;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
constexpr size_t kRasterViewWords = 20;
constexpr size_t kRasterViewBytes = 80;
static_assert(sizeof(std::array<float, kRasterViewWords>) == kRasterViewBytes);
static_assert(sizeof(std::array<uint32_t, 4>) == 16);
constexpr uint32_t kWidth = 1280;
constexpr uint32_t kHeight = 720;
constexpr uint64_t kDepthBytes = uint64_t{kWidth} * kHeight * sizeof(float);
constexpr std::streamoff kInputLimit = 512ll * 1024 * 1024;

[[noreturn]] void Fail(const std::string &message) {
  std::println(stderr, "{}", message);
  std::_Exit(1);
}

template <typename T> T Require(Native::Result<T> result) {
  if (!result) { Fail(result.error().Message); }
  return std::move(*result);
}

void Check(Native::Result<void> result) {
  if (!result) { Fail(result.error().Message); }
}

Bytes Read(const std::string &name) {
  std::ifstream file(name, std::ios::binary | std::ios::ate);
  if (!file) { Fail(name); }
  const auto size = file.tellg();
  if (size < 0 || size > kInputLimit) { Fail(name); }
  Bytes out(static_cast<size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
  if (!file) { Fail(name); }
  return out;
}

std::vector<uint32_t> Shader(const std::string &path) {
  auto words = outshine::Render::ReadSpirv(path);
  if (!words) { Fail(words.error()); }
  return std::move(*words);
}

std::unique_ptr<Native::Buffer>
Upload(Native::Device &device,
       Native::Commands &commands,
       const Bytes &bytes,
       Native::UseMask<Native::BufferUse> use = Native::BufferUse::Storage) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = use | Native::BufferUse::CopyDestination}));
  Check(Calibration::Upload(device, commands, *buffer, bytes));
  return buffer;
}

Bytes DepthPixels(Native::Device &device, const Native::Texture &depth) {
  auto buffer = Require(device.CreateBuffer({.Bytes = kDepthBytes,
                                             .Use = Native::BufferUse::CopyDestination,
                                             .Placement = Native::Memory::Readback}));
  auto commands = Require(device.Begin());
  Check(commands->Copy(depth,
                       *buffer,
                       {.RowBytes = uint64_t{kWidth} * sizeof(float),
                        .Region = {.Width = kWidth, .Height = kHeight}}));
  Check(Calibration::Finish(device, std::move(commands)));
  const auto data = Require(device.MapRead(*buffer, {.Bytes = kDepthBytes}));
  Bytes out(data.begin(), data.end());
  Check(device.Unmap(*buffer));
  return out;
}

Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}

} // namespace

int main(int argc, char *const argv[]) {
  if (argc != 2) { Fail("expected one prepared input directory"); }
  const std::string dir = argv[1];
  auto device = Require(Native::CreateDevice({.Validation = true}));
  std::println("DEVICE {}", device->Limits().Name);
  auto commands = Require(device->Begin());
  const auto positions =
      Upload(*device, *commands, Read(dir + "/road-expanded.bin"), Native::BufferUse::Vertex);
  const auto indices =
      Upload(*device, *commands, Read(dir + "/road-sequential.bin"), Native::BufferUse::Index);
  const auto models = Upload(*device, *commands, Read(dir + "/road-model.bin"));
  const auto scene = Upload(*device, *commands, Read(dir + "/frame-depth.bin"));
  const auto args =
      Upload(*device, *commands, Read(dir + "/road-args.bin"), Native::BufferUse::Indirect);
  const auto view = Read(dir + "/road-view.bin");
  Check(Calibration::Finish(*device, std::move(commands)));
  const auto vertex = Shader(dir + "/raster.spv");
  const auto fragment = Shader(dir + "/road-id.spv");
  const std::array streams{Native::VertexStream{.Stride = 16}};
  const std::array attrs{
      Native::VertexAttribute{.Location = 0, .Format = Native::VertexFormat::Float3},
      Native::VertexAttribute{.Location = 1, .Offset = 12, .Format = Native::VertexFormat::Uint32}};
  const std::array colours{
      Native::ColourTarget{.Texels = Native::Format::R32Float, .Rgb = {}, .Alpha = {}}};
  const auto pipeline = Require(device->CreateGraphicsPipeline(
      {.Vertex = {.Spirv = vertex},
       .Fragment = Native::ShaderCode{.Spirv = fragment},
       .Streams = streams,
       .Attributes = attrs,
       .Colours = colours,
       .Depth = Native::DepthTarget{.Compare = Native::DepthCompare::GreaterEqual},
       .Culling = Native::Cull::Back,
       .Label = "road-visible-id"}));
  const auto image = Require(device->CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = Native::Format::R32Float,
       .Use = Native::TextureUse::ColourAttachment | Native::TextureUse::CopySource}));
  const auto depth = Require(device->CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = Native::Format::Depth32Float,
       .Use = Native::TextureUse::DepthAttachment | Native::TextureUse::CopySource}));
  for (uint32_t mode = 0; mode < 2; ++mode) {
    commands = Require(device->Begin());
    const std::array colour{Native::ColourAttachment{.Image = {.Image = image.get()},
                                                     .Begin = Native::Load::Clear,
                                                     .End = Native::Store::Store}};
    Check(
        commands->BeginRenderPass({.Colours = colour,
                                   .Depth = Native::DepthAttachment{.Image = {.Image = depth.get()},
                                                                    .Begin = Native::Load::Clear,
                                                                    .End = Native::Store::Store,
                                                                    .Clear = 0}}));
    const std::array<uint32_t, 4> options{mode, 0, 0, 0};
    const std::array bindings{
        Storage(0, 0, *models),
        Storage(2, 0, *scene),
        Native::Binding{.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = view}},
        Native::Binding{.Set = 3,
                        .Slot = 0,
                        .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(options))}}};
    const std::array buffers{Native::VertexBuffer{
        .Stream = 0, .Data = positions.get(), .Range = {.Bytes = positions->Description().Bytes}}};
    Check(commands->DrawIndexedIndirect(
        {.State = {.Pipeline = pipeline.get(),
                   .Bindings = bindings,
                   .Vertices = buffers,
                   .Area = {.Width = kWidth, .Height = kHeight},
                   .Clip = {.Width = kWidth, .Height = kHeight}},
         .Indices = {.Data = indices.get(), .Range = {.Bytes = indices->Description().Bytes}},
         .Range = {.Data = args.get(), .Count = 1}}));
    Check(commands->EndRenderPass());
    Check(Calibration::Finish(*device, std::move(commands)));
    const auto pixels = DepthPixels(*device, *image);
    const auto z = DepthPixels(*device, *depth);
    std::ofstream id(dir + "/road-id-" + std::to_string(mode) + ".bin", std::ios::binary);
    id.write(reinterpret_cast<const char *>(pixels.data()),
             static_cast<std::streamsize>(pixels.size()));
    std::ofstream dz(dir + "/road-depth-" + std::to_string(mode) + ".bin", std::ios::binary);
    dz.write(reinterpret_cast<const char *>(z.data()), static_cast<std::streamsize>(z.size()));
    if (!id || !dz) { Fail("readback write failed"); }
    std::println("mode {} complete", mode);
  }
  return 0;
}
