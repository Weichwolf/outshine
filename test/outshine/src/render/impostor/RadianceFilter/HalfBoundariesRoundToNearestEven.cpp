#include "RadianceFilter.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

static_assert(sizeof(std::array<uint16_t, 4>) == 8);

struct Boundary {
  uint32_t Width;
  uint16_t Low;
};

void CheckBoundary(Device &device, const RadianceFilter &filter, Boundary boundary) {
  const auto width = boundary.Width;
  const auto low = boundary.Low;
  const size_t count = size_t{width} * width;
  std::vector<std::array<uint16_t, 4>> pixels(count);
  for (size_t i = 0; i < count; ++i) {
    for (size_t channel = 0; channel < 3; ++channel) {
      pixels[i][channel] = static_cast<uint16_t>(low + (i < count * (channel + 1) / 4 ? 1 : 0));
    }
    pixels[i][3] = 0x3c00;
  }
  const uint64_t bytes = count * sizeof(pixels.front());
  auto staging = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*staging, {.Bytes = bytes}));
  std::memcpy(mapped.data(), pixels.data(), static_cast<size_t>(bytes));
  Require(device.Unmap(*staging));
  auto source =
      Require(device.CreateTexture({.Width = width,
                                    .Height = width,
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto destination =
      Require(device.CreateTexture({.Width = 1,
                                    .Height = 1,
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Storage | TextureUse::CopySource}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *staging,
      *source,
      {.RowBytes = uint64_t{width} * 8, .Region = {.Width = width, .Height = width}}));
  Require(filter.Encode(*commands, *source, *destination));
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the production filter completes within the offline fixture deadline");
  const auto rows = ReadImage(device, *destination, {.Width = 1, .Height = 1});
  std::array<uint16_t, 4> actual{};
  std::memcpy(actual.data(), rows.Rows.data(), sizeof(actual));
  const auto high = static_cast<uint16_t>(low + 1);
  const auto even = static_cast<uint16_t>(low + (low & 1u));
  CHECK(actual[0] == low, "a quarter-step rounds down to the nearer half");
  CHECK(actual[1] == even, "an exact midpoint chooses the even half mantissa");
  CHECK(actual[2] == high, "a three-quarter-step rounds up to the nearer half");
  CHECK(actual[3] == 0x3c00, "full coverage remains exactly one");
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  RadianceFilter filter;
  Require(filter.Configure(*device));
  constexpr std::array<uint16_t, 11> boundaries{
      0x0000, 0x0001, 0x0002, 0x03fe, 0x03ff, 0x0400, 0x3bfe, 0x3bff, 0x3c00, 0x3c01, 0x7bfe};
  for (const uint32_t width : {2u, 8u}) {
    for (const auto low : boundaries) {
      CheckBoundary(*device, filter, {.Width = width, .Low = low});
    }
  }
  return Report();
}
