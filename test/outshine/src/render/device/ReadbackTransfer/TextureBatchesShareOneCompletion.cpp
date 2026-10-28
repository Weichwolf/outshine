#include "ReadbackTransfer.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

Completion Marker(Device &device) {
  return Require(device.Submit(Require(device.Begin())));
}

void Batches(Device &device) {
  constexpr std::array formats{Format::R8Unorm, Format::Rgba16Float, Format::Rgba32Float};
  constexpr std::array<uint64_t, 3> texelBytes{1, 8, 16};
  constexpr TextureRegion region{.X = 1, .Y = 1, .Width = 3, .Height = 3};
  std::array<std::unique_ptr<Texture>, 3> textures;
  std::array<TextureReadback, 3> sources;
  std::array<ReadbackSlice, 3> slices;
  std::array<std::vector<std::byte>, 3> expected;
  auto commands = Require(device.Begin());
  for (size_t at = 0; at < textures.size(); ++at) {
    textures[at] = Require(
        device.CreateTexture({.Width = 5,
                              .Height = 5,
                              .Texels = formats[at],
                              .Use = TextureUse::CopySource | TextureUse::CopyDestination}));
    const uint64_t row = 5 * texelBytes[at];
    auto upload = Require(device.CreateBuffer(
        {.Bytes = 5 * row, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
    const auto mapped = Require(device.MapWrite(*upload, {.Bytes = 5 * row}));
    for (size_t byte = 0; byte < mapped.size(); ++byte) {
      mapped[byte] = static_cast<std::byte>((byte + 17 * at) % 251);
    }
    for (uint64_t y = 1; y < 4; ++y) {
      const auto selected = mapped.subspan(y * row + texelBytes[at], 3 * texelBytes[at]);
      expected[at].insert(expected[at].end(), selected.begin(), selected.end());
    }
    Require(device.Unmap(*upload));
    Require(commands->Copy(
        *upload, *textures[at], {.RowBytes = row, .Region = {.Width = 5, .Height = 5}}));
    sources[at] = {.Source = textures[at].get(), .Region = region};
  }
  Require(device.Submit(std::move(commands)));
  const auto before = Marker(device);
  auto batch = Require(ReadbackTransfer::Start(device, sources, slices));
  const auto after = Marker(device);
  CHECK(after.Serial == before.Serial + 2, "all images add exactly one native submission");
  textures = {};
  auto moved = std::move(batch);
  const auto bytes = Require(moved.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes.has_value(), "moved batch completes after source release");
  if (!bytes) { return; }
  for (size_t at = 0; at < slices.size(); ++at) {
    const auto &slice = slices[at];
    CHECK(slice.Range.Offset % 16 == 0 && slice.RowBytes == 3 * texelBytes[at],
          "mixed formats and odd rows retain aligned starts and packed row pitches");
    const auto image = bytes->subspan(slice.Range.Offset, slice.Range.Bytes);
    CHECK(std::ranges::equal(image, expected[at]), "every selected image retains exact bytes");
  }
  const auto again = Require(moved.Poll(device));
  CHECK(again && again->data() == bytes->data(), "batch polling reuses its live mapping");
}

void Invalid(Device &device) {
  const auto texture =
      Require(device.CreateTexture({.Width = 3, .Height = 5, .Use = TextureUse::CopySource}));
  const TextureReadback valid{.Source = texture.get(), .Region = {.Width = 3, .Height = 5}};
  std::array sources{valid, TextureReadback{}};
  std::array<ReadbackSlice, 2> slices;
  const auto before = Marker(device);
  CHECK(!ReadbackTransfer::Start(device, {}, slices), "empty batch is rejected");
  CHECK(!ReadbackTransfer::Start(device, sources, std::span(slices).first(1)),
        "slice count must match image count");
  CHECK(!ReadbackTransfer::Start(device, sources, slices),
        "a missing later source rejects the batch");
  sources[1] = valid;
  sources[1].Region.X = 1;
  CHECK(!ReadbackTransfer::Start(device, sources, slices),
        "out-of-bounds later region is rejected");
  sources[1] = valid;
  sources[1].Region.Height = 0;
  CHECK(!ReadbackTransfer::Start(device, sources, slices),
        "zero height is rejected before division");

  class OversizedTexture final : public Texture {
  public:
    OversizedTexture()
        : Texture({.Width = UINT32_MAX,
                   .Height = UINT32_MAX,
                   .Texels = Format::Rgba32Float,
                   .Use = TextureUse::CopySource}) {}
  } oversized;

  sources[1] = {.Source = &oversized, .Region = {.Width = UINT32_MAX, .Height = UINT32_MAX}};
  CHECK(!ReadbackTransfer::Start(device, sources, slices),
        "image byte overflow is rejected before allocation");
  auto foreign = Require(CreateDevice({.Validation = true}));
  const auto foreignTexture =
      Require(foreign->CreateTexture({.Width = 3, .Height = 5, .Use = TextureUse::CopySource}));
  sources[1] = {.Source = foreignTexture.get(), .Region = valid.Region};
  CHECK(!ReadbackTransfer::Start(device, sources, slices),
        "a later image from another device cancels the unsubmitted batch");
  const auto after = Marker(device);
  CHECK(after.Serial == before.Serial + 1, "invalid batches submit no partial image transfers");
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Batches(*device);
  Invalid(*device);
  return Report();
}
