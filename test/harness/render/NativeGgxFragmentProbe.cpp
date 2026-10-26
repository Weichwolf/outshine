#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
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
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kPalettePrefix = 64, kCaseWords = 12;
constexpr uint32_t kMaximumCases = 16384;
constexpr uint64_t kRowAlignment = 256;
using Record = std::array<float, 8>;
using Counts = std::array<uint32_t, 4>;
static_assert(sizeof(Record) == 32 && sizeof(Counts) == 16);

std::vector<uint32_t> ReadPalette(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff size = file ? file.tellg() : std::streampos(-1);
  constexpr std::streamoff limit = (kPalettePrefix + kMaximumCases * kCaseWords) * sizeof(uint32_t);
  CHECK(size > 0 && size <= limit && size % sizeof(uint32_t) == 0, "palette bytes are bounded");
  if (size <= 0 || size > limit || size % sizeof(uint32_t) != 0) { Failed(); }
  std::vector<uint32_t> words(static_cast<size_t>(size) / sizeof(uint32_t));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(words.data()), size)) {
    CHECK(false, "palette bytes read");
    Failed();
  }
  return words;
}

void Capture(Device &device,
             const GraphicsPipeline &pipeline,
             std::span<const uint32_t> palette,
             uint32_t count,
             const char *outputPath) {
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
  const Counts counts{count, 0, 0, 0};
  const std::array bindings{
      Storage(0, 0, *paletteBuffer),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&counts, 1))}}};
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
        "fragment GGX capture completes within its wait budget");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = rowBytes * 2}));
  std::vector<Record> records(count);
  for (size_t i = 0; i < records.size(); ++i) {
    std::memcpy(records[i].data(),
                mapped.data() + i * sizeof(std::array<float, 4>),
                sizeof(std::array<float, 4>));
    std::memcpy(records[i].data() + 4,
                mapped.data() + rowBytes + i * sizeof(std::array<float, 4>),
                sizeof(std::array<float, 4>));
  }
  Require(device.Unmap(*readback));
  std::ofstream output(outputPath, std::ios::binary);
  const auto bytes = std::as_bytes(std::span(records));
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  CHECK(bool(output), "all fragment GGX records are saved");
}
} // namespace

int main(int argc, char *const *argv) {
  if (argc != 5) { return 2; }
  const auto palette = ReadPalette(argv[3]);
  if (palette.size() <= kPalettePrefix || (palette.size() - kPalettePrefix) % kCaseWords != 0) {
    return 2;
  }
  const auto count = static_cast<uint32_t>((palette.size() - kPalettePrefix) / kCaseWords);
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
  Capture(*device, *pipeline, palette, count, argv[4]);
  return Report();
}
