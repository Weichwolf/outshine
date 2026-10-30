#include "Check.h"
#include "GeometryProductCodec.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::vector<uint8_t> Bytes(std::span<const float> values) {
  std::vector<uint8_t> bytes(values.size_bytes());
  std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

void Storage() {
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
  const auto base = Bytes(values);
  ImageMipData mips;
  mips[0] = Bytes(std::span(values).first(4));
  Geometry geometry;
  CHECK(geometry.addImage({.WidthPx = 2,
                           .HeightPx = 2,
                           .Format = ImagePixelFormat::Rgba32Float,
                           .Rgba = base,
                           .LowerMips = ViewImageMips(mips)}),
        "geometry owns complete floating base and mip storage");
  const auto encoded = EncodeGeometryProduct(geometry, 4096);
  CHECK(encoded.has_value(), "the typed native product stores Float32 pixels");
  if (!encoded) { return; }
  const auto decoded = DecodeGeometryProduct(*encoded, encoded->size());
  CHECK(decoded.has_value(), "the typed Float32 product decodes");
  if (!decoded) { return; }
  const auto image = decoded->imageAt(0);
  CHECK(image.Format == ImagePixelFormat::Rgba32Float && std::ranges::equal(image.Rgba, base),
        "the source product preserves every Float32 bit without a transfer conversion");
  const auto stored = image.LowerMips[0];
  const auto prepared = mips[0];
  CHECK(stored && prepared, "stored floating mips remain owned");
  if (stored && prepared) {
    CHECK(std::ranges::equal(*stored, *prepared),
          "the existing typed codec preserves the full prepared Float32 mip");
  }
}
}

int main() {
  Storage();
  return outshine::Test::Report();
}
