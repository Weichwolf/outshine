#include <export/GltfExporter.h>
#ifdef OUTSHINE_GENERATE_H
#error Export must not depend on generator declarations
#endif
#include <scene/Geometry.h>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include "Check.h"

namespace {
uint32_t Word(std::span<const uint8_t> bytes, size_t at) {
  return uint32_t{bytes[at]} | (uint32_t{bytes[at + 1]} << 8u) | (uint32_t{bytes[at + 2]} << 16u) |
         (uint32_t{bytes[at + 3]} << 24u);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  CHECK(!exportGlb(geometry), "empty geometry produces an error rather than a file");
  Material material;
  material.BaseColour = {{0.25f, 0.5f, 0.75f, 1}};
  material.Metalness = 0.5f;
  material.Roughness = 0.25f;
  const auto surface = geometry.addSurface("native surface", material);
  const int part = geometry.addPart("triangle", surface);
  const std::array<float, 9> positions{0, 0, 0, 1, 0, 0, 0, 1, 0};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  CHECK(geometry.setPositions(part, positions) && geometry.setTriangles(part, indices),
        "native triangle is prepared");
  const auto output = exportGlb(geometry);
  CHECK(output.has_value(), "supported native material exports");
  if (output && output->size() >= 28) {
    const auto &bytes = *output;
    CHECK(Word(bytes, 0) == 0x46546c67 && Word(bytes, 4) == 2 && Word(bytes, 8) == bytes.size(),
          "GLB magic, version and complete byte length match the container specification");
    const size_t length = Word(bytes, 12);
    CHECK(length % 4 == 0 && Word(bytes, 16) == 0x4e4f534a && length <= bytes.size() - 28,
          "first chunk is aligned JSON with room for binary header");
    if (length <= bytes.size() - 28) {
      const std::string_view json(reinterpret_cast<const char *>(bytes.data() + 20), length);
      CHECK(json.contains("native surface") && json.contains("\"metallicFactor\":0.5") &&
                json.contains("\"roughnessFactor\":0.25") &&
                json.contains("\"baseColorFactor\":[0.25,0.5,0.75,1]"),
            "declared material name and factors are encoded directly in JSON");
      CHECK(Word(bytes, 24 + length) == 0x004e4942 &&
                Word(bytes, 20 + length) == bytes.size() - 28 - length,
            "binary chunk type and length consume exactly the remaining container");
    }
  } else {
    CHECK(false, "successful export must contain complete GLB headers");
  }
  for (const auto field : {&Material::Clearcoat,
                           &Material::Transmission,
                           &Material::SpecularFactor,
                           &Material::Roughness}) {
    Material unsupported = material;
    unsupported.*field = std::numeric_limits<float>::quiet_NaN();
    CHECK(geometry.setSurface(surface, unsupported), "prepare unsupported material");
    const auto failed = exportGlb(geometry);
    CHECK(!failed && !failed.error().empty(), "nonrepresentable material cannot silently export");
  }
  Material coated = material;
  coated.Clearcoat = 0.5f;
  CHECK(geometry.setSurface(surface, coated) && !exportGlb(geometry),
        "unimplemented material extension cannot be discarded");
  Material textured = material;
  textured.BaseColourMap.Image = 0;
  CHECK(geometry.setSurface(surface, textured) && !exportGlb(geometry),
        "native texture binding cannot disappear even when its image is missing");
  CHECK(geometry.setSurface(surface, material), "restore supported factors");
  const std::array<uint8_t, 4> pixel{255, 255, 255, 255};
  CHECK(geometry.addImage(1, 1, pixel) == 0 && !exportGlb(geometry),
        "unimplemented image export cannot report success");
  CHECK(geometry.images() == 1 && geometry.surfaceAt(surface) == material &&
            geometry.positionsOf(part).size() == positions.size(),
        "failed export preserves its native input");
  return Report();
}
