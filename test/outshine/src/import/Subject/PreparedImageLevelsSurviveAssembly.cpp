#include "Check.h"
#include "Subject.h"
#include <scene/Geometry.h>
#include <algorithm>
#include <array>

using namespace outshine;
using outshine::Test::Report;

int main() {
  Geometry geometry;
  const std::array<uint8_t, 16> base{};
  ImageMipData levels;
  levels[static_cast<size_t>(ImageMipKind::Colour)].emplace(4, 127);
  const auto image = geometry.addImage(
      {.WidthPx = 2, .HeightPx = 2, .Rgba = base, .LowerMips = ViewImageMips(levels)});
  CHECK(image, "native image owns a prepared colour chain");
  if (!image) { return Report(); }
  Material material;
  material.BaseColourMap.Image = *image;
  const auto surface = geometry.addSurface("prepared", material);
  CHECK(surface, "prepared image is bound to a native surface");
  if (!surface) { return Report(); }
  const auto part = geometry.addPart("triangle", *surface);
  CHECK(part && geometry.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "native geometry is complete");
  Gltf::Subject subject;
  CHECK(subject.Assemble(geometry), "native product reaches the shared import representation");
  geometry.clear();
  const auto images = subject.Images();
  CHECK(images.size() == 1 && images[0].LowerMips[1] && images[0].LowerMips[1]->front() == 127,
        "the import representation retains prepared bytes after the native owner is released");
  const auto restored = subject.Handed();
  CHECK(restored && restored->images() == 1 && restored->imageAt(0).LowerMips[1] &&
            std::ranges::equal(*restored->imageAt(0).LowerMips[1], *levels[1]),
        "conversion back to native products preserves prepared levels without filtering");
  return Report();
}
