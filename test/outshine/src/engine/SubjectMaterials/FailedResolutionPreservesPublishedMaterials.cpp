#include <array>
#include <cstdint>
#include <string>

#include <scene/Geometry.h>

#include "Check.h"
#include "Shape.h"
#include "SubjectMaterials.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Geometry geometry;
  const int image = *geometry.addImage(1, 1, std::array<uint8_t, 4>{31, 63, 127, 255});
  Material original;
  original.BaseColourMap.Image = image;
  const auto surface = geometry.addSurface("shared", original).value();
  for (int part = 0; part < 2; ++part) {
    const int at = geometry.addPart(part == 0 ? "first" : "second", surface).value();
    const float x = static_cast<float>(part);
    CHECK(geometry.setPositions(at, std::array<float, 9>{x, 0, 0, x + 1, 0, 0, x, 1, 0}),
          "positions");
    CHECK(geometry.setTriangles(at, std::array<uint32_t, 3>{0, 1, 2}), "triangles");
  }

  Render::ShapeStore storage;
  const auto shaped = Render::PrepareShape(geometry, storage);
  CHECK(shaped.has_value(), "native subject prepares a render shape");
  if (!shaped) { return Report(); }

  Core::SubjectMaterials materials;
  auto resolved = materials.Resolve(geometry, *shaped, {}, {}, -1, "subject");
  CHECK(resolved.has_value(), "valid native materials resolve");
  const auto published = materials.Slots();
  CHECK(published.size() == 1 && published[0].Colour.Rgba == geometry.imageAt(image).Rgba.data(),
        "published material borrows its owning geometry image");
  const auto *const publishedPixels = published[0].Colour.Rgba;

  Core::SubjectSurfaceOverride override;
  override.PartIndex = 1;
  override.RetainMaps = true;
  override.Surface.BaseColour = {{1, 0, 0, 1}};
  override.Surface.Unlit = true;
  resolved = materials.Resolve(geometry,
                               *shaped,
                               {},
                               std::span<const Core::SubjectSurfaceOverride>{&override, 1},
                               -1,
                               "subject");
  CHECK(resolved.has_value(), "part override resolves");
  CHECK(materials.PartSlots().size() == 2 && materials.PartSlots()[0] != materials.PartSlots()[1],
        "part override splits a material shared by two parts");
  const uint32_t overridden = materials.PartSlots()[1];
  CHECK(overridden < materials.Slots().size() &&
            materials.Slots()[overridden].Colour.Rgba == publishedPixels &&
            materials.Slots()[overridden].Row.BaseColour[0] == 1.0f,
        "part override retains maps and replaces the metallic-roughness row");

  const auto committedSlots = materials.PartSlots();
  override.PartIndex = 7;
  resolved = materials.Resolve(geometry,
                               *shaped,
                               {},
                               std::span<const Core::SubjectSurfaceOverride>{&override, 1},
                               -1,
                               "subject");
  CHECK(!resolved.has_value(), "unmatched override rejects resolution");
  CHECK(materials.PartSlots().size() == committedSlots.size() &&
            materials.PartSlots()[1] == committedSlots[1],
        "unmatched override preserves the committed part mapping");
  return Report();
}
