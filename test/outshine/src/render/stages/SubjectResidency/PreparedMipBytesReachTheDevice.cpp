#include "Check.h"
#include "SubjectResidency.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test::Gpu;
using outshine::Test::Report;

namespace {
constexpr std::array<uint8_t, 4> kColourMip{20, 40, 60, 80};
constexpr std::array<uint8_t, 4> kNormalMip{120, 130, 255, 100};

std::optional<std::vector<uint8_t>>
ReadLevel(Device *device, Texture *image, uint32_t level, uint32_t size) {
  auto readback = RetryOffline(
      [&] {
        return ReadbackTransfer::Start(
            *device, *image, {.Mip = level, .Width = size, .Height = size});
      },
      std::chrono::seconds(5));
  if (!readback) { return std::nullopt; }
  const auto bytes = readback->Wait(*device, std::chrono::seconds(5));
  if (!bytes || !*bytes) { return std::nullopt; }
  std::vector<uint8_t> result((**bytes).size());
  std::memcpy(result.data(), (**bytes).data(), result.size());
  return result;
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  {
    SubjectResidency residency;
    Require(residency.StandsOn(device.get()));
    const std::array<uint8_t, 16> base{
        255, 255, 255, 7, 255, 255, 255, 7, 255, 255, 255, 7, 255, 255, 255, 7};
    ImageMipData prepared;
    prepared[1] = std::vector<uint8_t>(kColourMip.begin(), kColourMip.end());
    prepared[2] = std::vector<uint8_t>(kNormalMip.begin(), kNormalMip.end());
    SubjectTexture texture{.Rgba = base.data(),
                           .Width = 2,
                           .Height = 2,
                           .LowerMips = ViewImageMips(prepared),
                           .Uv = {}};
    const auto colour =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(colour, "prepared colour image uploads");
    if (!colour) { return Report(); }
    const auto colourMip = ReadLevel(device.get(), colour->Image.get(), 1, 1);
    CHECK(colourMip && std::ranges::equal(*colourMip, kColourMip),
          "GPU receives stored colour levels rather than re-filtering the white base");
    const auto normal =
        residency.Upload(texture, SubjectResidency::Transfer::Linear, TexelKind::Direction);
    CHECK(normal, "prepared normal image uploads");
    if (!normal) { return Report(); }
    const auto normalMip = ReadLevel(device.get(), normal->Image.get(), 1, 1);
    CHECK(normalMip && std::ranges::equal(*normalMip, kNormalMip),
          "normal confidence bytes reach the GPU unchanged");
    const auto normalBase = ReadLevel(device.get(), normal->Image.get(), 0, 2);
    CHECK(normalBase && (*normalBase)[3] == 255 && (*normalBase)[15] == 255,
          "normal base texels retain the existing unit-confidence contract");
    static_cast<void>(residency.TakeUploadBytes());
    texture.Mip = SubjectMip::None;
    const auto single =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(single && residency.TakeUploadBytes() == 16,
          "disabled mip sampling uploads only the base");
    prepared[1] = std::vector<uint8_t>(3);
    texture.LowerMips = ViewImageMips(prepared);
    CHECK(!residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value),
          "malformed prepared levels cannot publish a sampled texture");
  }
  return Report();
}
