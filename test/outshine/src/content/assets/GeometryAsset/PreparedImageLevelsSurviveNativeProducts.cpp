#include "Check.h"
#include "content/GeometryAsset.h"
#include <algorithm>
#include <array>
#include <vector>

using namespace outshine;
using outshine::Test::Report;

int main() {
  Geometry geometry;
  const std::array<uint8_t, 60> base{};
  ImageMipData levels;
  for (size_t kind = 0; kind < levels.size(); ++kind) {
    levels[kind].emplace(12, static_cast<uint8_t>(kind + 1));
  }
  const auto image = geometry.addImage(
      {.WidthPx = 5, .HeightPx = 3, .Rgba = base, .LowerMips = ViewImageMips(levels)});
  CHECK(image, "an odd-sized native image accepts complete lower mip levels");
  const auto material = geometry.addSurface("prepared", Material{});
  CHECK(material, "prepared image has a native material owner");
  if (!material) { return Report(); }
  const auto part = geometry.addPart("triangle", *material);
  CHECK(part && geometry.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "prepared image belongs to a complete native product");
  for (auto &bytes : levels) { std::ranges::fill(*bytes, uint8_t{99}); }
  const auto stored = geometry.imageAt(0);
  for (size_t kind = 0; kind < levels.size(); ++kind) {
    CHECK(stored.LowerMips[kind] && stored.LowerMips[kind]->front() == kind + 1,
          "the native owner copies each interpretation independently");
  }
  const auto encoded = EncodeGeometryAsset(geometry, 4096);
  CHECK(encoded && (*encoded)[5] == 2, "prepared images select the extended native codec");
  if (!encoded) { return Report(); }
  auto restored = DecodeGeometryAsset(*encoded, encoded->size());
  CHECK(restored, "prepared native product loads without image filtering or source data");
  if (restored) {
    const auto after = restored->imageAt(0);
    CHECK(after.valid() && std::ranges::equal(after.Rgba, base), "base bytes remain unchanged");
    for (size_t kind = 0; kind < levels.size(); ++kind) {
      CHECK(after.LowerMips[kind] &&
                std::ranges::equal(*stored.LowerMips[kind], *after.LowerMips[kind]),
            "every prepared lower level survives the native product exactly");
    }
    auto clone = restored->clone();
    restored->clear();
    CHECK(clone.imageAt(0).LowerMips[0]->front() == 1,
          "native clones retain independent prepared mip ownership");
  }
  auto corrupt = *encoded;
  corrupt[96] = 8;
  CHECK(!DecodeGeometryAsset(corrupt, corrupt.size()), "unknown prepared interpretations fail");
  for (size_t count = 0; count < encoded->size(); count += 7) {
    CHECK(!DecodeGeometryAsset(std::span(*encoded).first(count), encoded->size()),
          "truncated mip metadata or texels cannot publish a partial product");
  }
  return Report();
}
