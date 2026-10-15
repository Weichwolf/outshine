#include "content/GeometryAsset.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t kBytesMost = 4096;
  constexpr std::array<float, 6> uv{0, 1, 1, 1, 0, 2};
  constexpr std::array<float, 6> metres{-1, 0, 3, 0, -1, 3};
  for (const auto pattern :
       {SurfacePattern::None, SurfacePattern::Facade, SurfacePattern::FacadeMetres}) {
    Geometry original;
    Material material;
    material.Pattern = pattern;
    const auto surface = original.addSurface("facade", material);
    CHECK(surface.has_value(), "the explicit material mode is valid");
    if (!surface) { continue; }
    const auto part = original.addPart("wall", *surface);
    CHECK(part.has_value(), "the native product has a wall part");
    if (!part) { continue; }
    CHECK(original.setPositions(*part, std::array<float, 9>{0, 0, 0, 3, 0, 0, 0, 3, 0}) &&
              original.setTexture(*part, uv) && original.setTexture(*part, metres, 1) &&
              original.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "the native wall carries distinct metadata and physical coordinates");
    const auto bytes = EncodeGeometryAsset(original, kBytesMost);
    CHECK(bytes.has_value(), "the material and both streams encode");
    if (!bytes) { continue; }
    original = Geometry{};
    const auto loaded = DecodeGeometryAsset(*bytes, kBytesMost);
    CHECK(loaded.has_value(), "a fresh owner decodes without the original geometry");
    if (!loaded) { continue; }
    CHECK(loaded->surfaceAt(MaterialInstance(0)).Pattern == pattern &&
              std::ranges::equal(loaded->textureOf(0), uv) &&
              std::ranges::equal(loaded->textureOf(0, UvSet::Uv1), metres),
          "the exact procedural mode and independent coordinates survive the cache");
    CHECK(EncodeGeometryAsset(*loaded, kBytesMost) == bytes,
          "the owned roundtrip remains canonical for every material mode");
  }
  return Report();
}
