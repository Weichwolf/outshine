#include "NativeMaterialCapture.h"
#include "NativeMaterialCaptureTexture.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <bit>
#include <chrono>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <optional>
#include <utility>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kMaximumCases = 16384;
constexpr uint64_t kRowAlignment = 256;

void Capture(Device &device,
             const GraphicsPipeline &pipeline,
             std::span<const uint32_t> palette,
             uint32_t count,
             const char *outputPath,
             uint32_t textureSide,
             const char *texturePath) {
  auto commands = Require(device.Begin());
  const auto paletteBuffer = outshine::Test::Gpu::Upload(device, *commands, palette);
  const uint64_t rowBytes =
      ((count * sizeof(std::array<float, 4>) + kRowAlignment - 1) / kRowAlignment) * kRowAlignment;
  auto readback = Require(device.CreateBuffer(
      {.Bytes = rowBytes * 2, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  std::array<std::unique_ptr<Texture>, 2> images;
  std::array<ColourAttachment, 2> colours;
  for (size_t i = 0; i < images.size(); ++i) {
    images[i] = Require(
        device.CreateTexture({.Width = count,
                              .Height = 1,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    colours[i] = {.Image = {.Image = images[i].get()}, .Begin = Load::Clear};
  }
  const CaptureUniform parameters{.Count = count};
  std::vector bindings{
      Storage(0, 0, *paletteBuffer),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&parameters, 1))}}};
  CaptureTexture texture;
  if (texturePath != nullptr) {
    texture = UploadCaptureTexture(device, *commands, texturePath, textureSide);
    bindings.push_back({.Set = 0,
                        .Slot = 1,
                        .Value = SampledTexture{
                            .Image = {.Image = texture.Image.get(),
                                      .Mips = static_cast<uint32_t>(std::bit_width(textureSide))},
                            .Sampling = texture.Sampling.get()}});
  }
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = static_cast<float>(count), .Height = 1},
                                    .Clip = {.Width = count, .Height = 1}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  for (size_t i = 0; i < images.size(); ++i) {
    Require(commands->Copy(*images[i],
                           *readback,
                           {.BufferOffset = i * rowBytes,
                            .RowBytes = rowBytes,
                            .Region = {.Width = count, .Height = 1}}));
  }
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "material fragment capture completes within its wait budget");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = rowBytes * 2}));
  std::vector<CapturePixel> records(count);
  for (size_t i = 0; i < records.size(); ++i) {
    std::memcpy(records[i].First.data(),
                mapped.data() + i * sizeof(std::array<float, 4>),
                sizeof(std::array<float, 4>));
    std::memcpy(records[i].Second.data(),
                mapped.data() + rowBytes + i * sizeof(std::array<float, 4>),
                sizeof(std::array<float, 4>));
  }
  Require(device.Unmap(*readback));
  std::ofstream output(outputPath, std::ios::binary);
  const auto bytes = std::as_bytes(std::span(records));
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  CHECK(bool(output), "all material fragment records are saved");
}
} // namespace

int main(int argc, char *const *argv) {
  if (argc != 6 && argc != 8) { return 2; }
  const std::string_view argument(argv[5]);
  uint32_t count = 0;
  const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), count);
  if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size() || count == 0 ||
      count > kMaximumCases) {
    return 2;
  }
  uint32_t textureSide = 0;
  if (argc == 8) {
    const std::string_view extent(argv[7]);
    const auto side = std::from_chars(extent.data(), extent.data() + extent.size(), textureSide);
    if (side.ec != std::errc{} || side.ptr != extent.data() + extent.size() || textureSide == 0 ||
        textureSide > 512 || !std::has_single_bit(textureSide)) {
      return 2;
    }
  }
  const auto palette = ReadCaptureWords(argv[3]);
  if (textureSide != 0 &&
      palette.size() * sizeof(uint32_t) != count * sizeof(CaptureTextureQuery)) {
    return 2;
  }
  auto device = Require(CreateDevice({.Validation = true}));
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}},
                           ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  const auto pipeline =
      Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = Spirv(argv[1])},
                                              .Fragment = ShaderCode{.Spirv = Spirv(argv[2])},
                                              .Streams = {},
                                              .Attributes = {},
                                              .Colours = targets,
                                              .Depth = std::nullopt,
                                              .Culling = Cull::None,
                                              .Label = {}}));
  Capture(*device, *pipeline, palette, count, argv[4], textureSide, argc == 8 ? argv[6] : nullptr);
  return Report();
}
