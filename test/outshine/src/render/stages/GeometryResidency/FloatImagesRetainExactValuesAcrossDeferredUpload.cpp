#include "Check.h"
#include "GeometryResidency.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Render::Native;
  using namespace outshine::Test;
  using namespace outshine::Test::Gpu;
  const std::array<float, 16> values{.12345679f,
                                     65537,
                                     -2.5f,
                                     1,
                                     .12345679f,
                                     65537,
                                     -2.5f,
                                     1,
                                     .12345679f,
                                     65537,
                                     -2.5f,
                                     1,
                                     .12345679f,
                                     65537,
                                     -2.5f,
                                     1};
  std::vector<uint8_t> base(sizeof(values));
  std::memcpy(base.data(), values.data(), base.size());
  std::vector<uint8_t> lower(4 * sizeof(float));
  std::memcpy(lower.data(), values.data(), lower.size());
  auto device = Require(CreateDevice({.Validation = true}));
  GeometryResidency residency;
  Require(residency.StandsOn(device.get()));
  for (const auto transfer :
       {GeometryResidency::Transfer::Linear, GeometryResidency::Transfer::Srgb}) {
    GeometryResidency::BoundImage image;
    {
      auto pixels = base;
      ImageMipData mips;
      mips[0] = lower;
      const MaterialTexture source{.Rgba = pixels.data(),
                                   .Width = 2,
                                   .Height = 2,
                                   .Format = ImagePixelFormat::Rgba32Float,
                                   .LowerMips = ViewImageMips(mips),
                                   .Magnify = outshine::Filter::Nearest,
                                   .Minify = outshine::Filter::Nearest,
                                   .Mip = MipFilter::Nearest,
                                   .Uv = {}};
      image = Require(residency.Upload(
          source, transfer, TexelKind::Value, GeometryResidency::ImageUploadMode::Deferred));
      CHECK(image.Image->Description().Texels == Format::Rgba32Float &&
                image.Image->Description().Mips == 2,
            "floating colour uploads remain linear Float32 for every transfer intent");
    }
    Require(residency.SubmitPendingUploads());
    CHECK(residency.TakeUploadBytes() == base.size() + lower.size(),
          "accepted transfer bytes include all Float32 components of both levels");
    for (uint32_t mip = 0; mip < 2; ++mip) {
      auto readback = Require(ReadbackTransfer::Start(
          *device, *image.Image, {.Mip = mip, .Width = 2u >> mip, .Height = 2u >> mip}));
      const auto read = Require(readback.Wait(*device, std::chrono::seconds(5)));
      const auto &expected = mip == 0 ? base : lower;
      CHECK(
          read && std::ranges::equal(*read, std::as_bytes(std::span(expected))),
          "native Float32 texels survive source release, mip offsets and transfer intent exactly");
    }
  }
  const std::array<float, 4> normal{.12345679f, .5f, 1, .25f};
  std::vector<uint8_t> normalBytes(sizeof(normal));
  std::memcpy(normalBytes.data(), normal.data(), normalBytes.size());
  const MaterialTexture direction{.Rgba = normalBytes.data(),
                                  .Width = 1,
                                  .Height = 1,
                                  .Format = ImagePixelFormat::Rgba32Float,
                                  .Magnify = outshine::Filter::Nearest,
                                  .Minify = outshine::Filter::Nearest,
                                  .Mip = MipFilter::None,
                                  .Uv = {}};
  const auto normalImage = Require(
      residency.Upload(direction, GeometryResidency::Transfer::Linear, TexelKind::Direction));
  auto normalReadback =
      Require(ReadbackTransfer::Start(*device, *normalImage.Image, {.Width = 1, .Height = 1}));
  const auto normalRead = Require(normalReadback.Wait(*device, std::chrono::seconds(5)));
  const std::array<float, 4> expectedNormal{normal[0], normal[1], normal[2], 1};
  CHECK(normalRead && std::ranges::equal(*normalRead, std::as_bytes(std::span(expectedNormal))),
        "Float32 normal upload preserves RGB while replacing source alpha with unit moment weight");
  MaterialTexture missing;
  missing.Format = ImagePixelFormat::Rgba32Float;
  const auto invalid =
      residency.Upload(missing, GeometryResidency::Transfer::Linear, TexelKind::Value);
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "a missing Float32 image cannot reinterpret the four-byte default");
  return Report();
}
