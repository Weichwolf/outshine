#include "Check.h"
#include "GeometryProductCodec.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using namespace outshine;
using outshine::Test::Report;

int main() {
  constexpr auto format = ImagePixelFormat::Rgba16Float;
  std::array<uint8_t, 32> base{};
  for (size_t at = 0; at < base.size(); ++at) { base[at] = static_cast<uint8_t>(at); }
  ImageMipData mips;
  constexpr uint8_t kStoredMipByte = 37;
  mips[0].emplace(8, kStoredMipByte);
  const ImageView view{.WidthPx = 2,
                       .HeightPx = 2,
                       .Format = format,
                       .Rgba = base,
                       .LowerMips = ViewImageMips(mips)};
  CHECK(view.valid() && LowerImageMipBytes(2, 2, format) == 8 &&
            LowerImageMipBytes(5, 3, format) == 24,
        "typed storage sizes every prepared level in bytes");
  auto invalid = view;
  invalid.Format = static_cast<ImagePixelFormat>(255);
  CHECK(!invalid.valid() && !LowerImageMipBytes(2, 2, invalid.Format),
        "unknown image formats cannot publish an ambiguous byte interpretation");
  invalid = view;
  invalid.Rgba = std::span(base).first(base.size() - 1);
  CHECK(!invalid.valid(), "a truncated half-float base cannot become a valid image");
  invalid = view;
  invalid.Format = ImagePixelFormat::Rgba8Unorm;
  CHECK(!invalid.valid(), "prepared mip sizes must match the declared format");
  Geometry geometry;
  const auto image = geometry.addImage(view);
  CHECK(image, "native geometry accepts owned half-float pixels and levels");
  if (!image) { return Report(); }
  const auto original = base;
  const std::array<uint8_t, 4> ordinary{1, 2, 3, 4};
  CHECK(geometry.addImage(1, 1, ordinary), "ordinary RGBA8 remains valid beside typed images");
  base.fill(0);
  mips = {};
  const auto held = geometry.imageAt(*image);
  const auto heldMips = held.LowerMips[0];
  CHECK(heldMips.has_value(), "the native owner retains its prepared linear mips");
  if (!heldMips) { return Report(); }
  CHECK(held.Format == format && std::ranges::equal(held.Rgba, original) && heldMips->size() == 8,
        "the owner retains the format and independent base/mip allocations");
  const auto encoded = EncodeGeometryProduct(geometry, 4096);
  CHECK(encoded && (*encoded)[5] == 7,
        "typed image bytes select the new version instead of an RGBA8 schema");
  if (!encoded) { return Report(); }
  auto restored = DecodeGeometryProduct(*encoded, encoded->size());
  CHECK(restored.has_value(), "the typed native product decodes");
  if (!restored) { return Report(); }
  const auto after = restored->imageAt(0);
  const auto afterMips = after.LowerMips[0];
  CHECK(afterMips.has_value(), "the decoded product retains its prepared linear mips");
  if (!afterMips) { return Report(); }
  CHECK(after.Format == format && std::ranges::equal(after.Rgba, original) &&
            std::ranges::equal(*afterMips, *heldMips),
        "native asset roundtrip preserves each declared byte without conversion");
  CHECK(restored->images() == 2 && restored->imageAt(1).Format == ImagePixelFormat::Rgba8Unorm &&
            std::ranges::equal(restored->imageAt(1).Rgba, ordinary),
        "mixed image tables retain each image format instead of applying one global format");
  const auto clone = restored->clone();
  restored->clear();
  CHECK(clone.imageAt(0).valid() && clone.imageAt(0).Format == format &&
            std::ranges::equal(clone.imageAt(0).Rgba, original),
        "cloning preserves typed image ownership after the source is cleared");
  auto corrupt = *encoded;
  constexpr size_t kFormatOffset = sizeof(uint64_t) + 6 * sizeof(int);
  corrupt[kFormatOffset] = 255;
  CHECK(!DecodeGeometryProduct(corrupt, corrupt.size()),
        "an unknown serialized image format cannot be read as default RGBA8");
  for (size_t count = 0; count < encoded->size(); ++count) {
    CHECK(!DecodeGeometryProduct(std::span(*encoded).first(count), encoded->size()),
          "no truncated typed image or mip payload can publish a partial native product");
  }
  return Report();
}
