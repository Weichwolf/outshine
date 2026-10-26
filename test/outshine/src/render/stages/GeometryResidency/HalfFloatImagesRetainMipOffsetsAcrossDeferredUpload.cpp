#include "Check.h"
#include "GeometryResidency.h"
#include "ReadbackTransfer.h"
#include "HalfTexels.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Render::Native;
  using namespace outshine::Test;
  using namespace outshine::Test::Gpu;
  constexpr std::array<float, 16> values{
      0, .0002f, .5f, 1, 0, .0002f, .5f, 1, 0, .0002f, .5f, 1, 0, .0002f, .5f, 1};
  const auto base = Core::PackHalfTexels({.WidthPx = 2, .HeightPx = 2}, values);
  const auto lower =
      Core::PackHalfTexels({.WidthPx = 1, .HeightPx = 1}, std::span(values).first(4));
  CHECK(base && lower, "linear half-float fixture pixels are available");
  if (!base || !lower) { return Report(); }
  auto device = Require(CreateDevice({.Validation = true}));
  GeometryResidency residency;
  Require(residency.StandsOn(device.get()));
  GeometryResidency::BoundImage image;
  {
    auto source = *base;
    ImageMipData mips;
    mips[0] = *lower;
    const SubjectTexture texture{.Rgba = source.data(),
                                 .Width = 2,
                                 .Height = 2,
                                 .Format = ImagePixelFormat::Rgba16Float,
                                 .LowerMips = ViewImageMips(mips),
                                 .Uv = {}};
    image = Require(residency.Upload(texture,
                                     GeometryResidency::Transfer::Linear,
                                     TexelKind::Value,
                                     GeometryResidency::ImageUploadMode::Deferred));
    CHECK(image.Image->Description().Texels == Format::Rgba16Float &&
              image.Image->Description().Mips == 2,
          "declared image format and both levels reach the native texture");
    source.clear();
    mips = {};
  }
  const size_t bytes = base->size() + lower->size();
  CHECK(residency.Allocations().TransferBytes == bytes,
        "deferred staging owns exactly all half-float base and mip bytes");
  Require(residency.SubmitPendingUploads());
  CHECK(residency.TakeUploadBytes() == bytes,
        "the accepted transfer accounts for eight bytes per texel");
  for (uint32_t mip = 0; mip < 2; ++mip) {
    auto transfer = Require(ReadbackTransfer::Start(
        *device, *image.Image, {.Mip = mip, .Width = 2u >> mip, .Height = 2u >> mip}));
    const auto read = Require(transfer.Wait(*device, std::chrono::seconds(5)));
    const auto &expected = mip == 0 ? *base : *lower;
    CHECK(read && std::ranges::equal(*read, std::as_bytes(std::span(expected))),
          "every half-float mip survives source release and its exact upload offset");
  }
  SubjectTexture missing;
  missing.Format = ImagePixelFormat::Rgba16Float;
  const auto invalid =
      residency.Upload(missing, GeometryResidency::Transfer::Linear, TexelKind::Value);
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "a missing typed image cannot reinterpret the four-byte default as half floats");
  return Report();
}
