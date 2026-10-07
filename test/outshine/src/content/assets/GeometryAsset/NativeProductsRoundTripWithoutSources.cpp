#include "content/GeometryAsset.h"
#include "content/AssetGeneration.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <limits>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Product() {
  Geometry geometry;
  const std::array<uint8_t, 16> rgba = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  const auto image = geometry.addImage(2, 2, rgba);
  CHECK(image && *image == 0, "native image uses the first owner-local slot");
  Material surface;
  surface.Pattern = SurfacePattern::Facade;
  surface.BaseColour = {{0.2f, 0.3f, 0.4f, 0.5f}};
  surface.Metalness = 0.2f;
  surface.Roughness = 0.4f;
  surface.Transmission = 0.1f;
  surface.Ior = 1.4f;
  surface.Emission = {{0.2f, 0.1f, 0.3f}};
  surface.Alpha = AlphaMode::Masked;
  surface.DoubleSided = true;
  surface.CoverageCut = 0.25f;
  surface.Unlit = true;
  surface.NeedsTangents = true;
  surface.NormalScale = 0.3f;
  surface.OcclusionStrength = 0.6f;
  surface.SpecularFactor = 0.4f;
  surface.SpecularColour = {{0.7f, 0.8f, 0.9f}};
  surface.SheenColour = {{0.1f, 0.2f, 0.3f}};
  surface.SheenRoughness = 0.3f;
  surface.Clearcoat = 0.7f;
  surface.ClearcoatRoughness = 0.2f;
  surface.Anisotropy = 0.1f;
  surface.AnisotropyRotationRad = 0.3f;
  surface.Iridescence = 0.4f;
  surface.IridescenceIor = 1.5f;
  surface.IridescenceThicknessMinNm = 110.0f;
  surface.IridescenceThicknessMaxNm = 220.0f;
  surface.Thickness = 0.8f;
  surface.AttenuationDistance = 15.0f;
  surface.AttenuationColour = {{0.6f, 0.5f, 0.4f}};
  int mapIndex = 0;
  for (auto *map : {&surface.BaseColourMap,
                    &surface.NormalMap,
                    &surface.MetalRoughMap,
                    &surface.EmissiveMap,
                    &surface.OcclusionMap,
                    &surface.SpecularStrengthMap,
                    &surface.SpecularTintMap}) {
    map->Image = *image;
    map->Set = mapIndex % 2 != 0 ? UvSet::Uv1 : UvSet::Uv0;
    map->Sampler = {.Magnify = Filter::Nearest,
                    .Minify = Filter::Nearest,
                    .Mip = MipFilter::Nearest,
                    .WrapU = Wrap::ClampToEdge,
                    .WrapV = Wrap::MirroredRepeat};
    map->Uv = {.OffsetUv = {{mapIndex * 0.1, -0.2}},
               .RotationRad = mapIndex * 0.2,
               .ScaleUv = {{2.0, -3.0}}};
    ++mapIndex;
  }
  const auto material = geometry.addSurface("all material features", surface);
  CHECK(material, "full native material is accepted");
  const auto defaultMaterial = geometry.addSurface("infinite attenuation", Material{});
  CHECK(defaultMaterial, "the native default permits infinite attenuation distance");
  const auto part = geometry.addPart("road / water / facade", *material);
  CHECK(part, "native mesh part is accepted");
  const std::array<float, 9> xyz = {0, 0, 0, 1, 0, 0, 0, 0, 1};
  const std::array<float, 9> normals = {0, 1, 0, 0, 1, 0, 0, 1, 0};
  const std::array<float, 6> uv0 = {0, 0, 1, 0, 0, 1};
  const std::array<float, 6> uv1 = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f};
  const std::array<float, 12> tangents = {1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1};
  const std::array<float, 12> colours = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
  const std::array<uint32_t, 3> indices = {0, 2, 1};
  CHECK(geometry.setPositions(*part, xyz) && geometry.setNormals(*part, normals) &&
            geometry.setTexture(*part, uv0) && geometry.setTexture(*part, uv1, 1) &&
            geometry.setTangents(*part, tangents) && geometry.setColours(*part, colours) &&
            geometry.setTriangles(*part, indices) &&
            geometry.setPlacement(*part, Mat4ShiftedBy({{2, 3, 4}})),
        "all native mesh attributes and model placement are accepted");
  const PunctualLight light{.Kind = LightKind::Spot,
                            .Colour = {{0.2f, 0.3f, 0.4f}},
                            .Intensity = 17.0f,
                            .Position = {{1, 2, 3}},
                            .Direction = {{0, -1, 0}},
                            .InnerConeRad = 0.1f,
                            .OuterConeRad = 0.4f,
                            .RangeM = 20.0f};
  CHECK(geometry.addLamp("street light", light, Mat4ShiftedBy({{5, 6, 7}})),
        "native light data and placement are accepted");
  CHECK(geometry.wellFormed(), "the generated product is a complete renderable scene");
  return geometry;
}

void Compare(const Geometry &a, const Geometry &b) {
  CHECK(a.parts() == b.parts() && a.surfaces() == b.surfaces() && a.images() == b.images() &&
            a.lamps() == b.lamps(),
        "all native scene tables survive");
  for (int index = 0; index < a.surfaces(); ++index) {
    CHECK(a.surfaceAt(MaterialInstance(index)) == b.surfaceAt(MaterialInstance(index)) &&
              a.surfaceNameOf(index) == b.surfaceNameOf(index),
          "all material factors, seven maps, UV transforms, samplers and names survive exactly");
  }
  for (int index = 0; index < a.images(); ++index) {
    const auto before = a.imageAt(index), after = b.imageAt(index);
    CHECK(before.WidthPx == after.WidthPx && before.HeightPx == after.HeightPx &&
              std::ranges::equal(before.Rgba, after.Rgba),
          "native RGBA pixels survive without colour conversion");
  }
  for (int part = 0; part < a.parts(); ++part) {
    CHECK(a.nameOf(part) == b.nameOf(part) && a.materialOf(part) == b.materialOf(part) &&
              a.placementOf(part) == b.placementOf(part),
          "owner-local binding, placement and part names survive");
    CHECK(std::ranges::equal(a.positionsOf(part), b.positionsOf(part)) &&
              std::ranges::equal(a.normalsOf(part), b.normalsOf(part)) &&
              std::ranges::equal(a.textureOf(part), b.textureOf(part)) &&
              std::ranges::equal(a.textureOf(part, UvSet::Uv1), b.textureOf(part, UvSet::Uv1)) &&
              std::ranges::equal(a.tangentsOf(part), b.tangentsOf(part)) &&
              std::ranges::equal(a.coloursOf(part), b.coloursOf(part)) &&
              std::ranges::equal(a.trianglesOf(part), b.trianglesOf(part)),
          "all mesh attributes retain exact values and topology");
  }
  const auto &before = a.lampAt(0), &after = b.lampAt(0);
  CHECK(before.Kind == after.Kind && before.Colour == after.Colour &&
            before.Intensity == after.Intensity && before.Position == after.Position &&
            before.Direction == after.Direction && before.InnerConeRad == after.InnerConeRad &&
            before.OuterConeRad == after.OuterConeRad && before.RangeM == after.RangeM &&
            a.lampNameOf(0) == b.lampNameOf(0) && a.lampPlacementOf(0) == b.lampPlacementOf(0),
        "native photometric light and placement survive");
}
}

int main() {
  const auto empty = EncodeGeometryAsset(Geometry{}, 24);
  const auto emptyProduct = empty ? DecodeGeometryAsset(*empty, 24) : std::nullopt;
  CHECK(emptyProduct && emptyProduct->parts() == 0 && emptyProduct->lamps() == 0,
        "a known empty native scene remains distinct from corruption or an absent asset");
  const Geometry original = Product();
  const auto encoded = EncodeGeometryAsset(original, 1u << 20u);
  CHECK(encoded, "a complete native product encodes within its byte bound");
  if (!encoded) { return Report(); }
  auto decoded = DecodeGeometryAsset(*encoded, encoded->size());
  CHECK(decoded, "the complete native product decodes without source data or a device");
  if (decoded) { Compare(original, *decoded); }
  CHECK(!EncodeGeometryAsset(original, encoded->size() - 1) &&
            !DecodeGeometryAsset(*encoded, encoded->size() - 1),
        "encoding and decoding respect the caller's exact payload bound");
  auto corrupt = *encoded;
  corrupt.push_back(0);
  CHECK(!DecodeGeometryAsset(corrupt, corrupt.size()), "trailing bytes are not a native product");
  corrupt = *encoded;
  std::fill_n(corrupt.begin() + 8, 4, uint8_t{255});
  CHECK(!DecodeGeometryAsset(corrupt, corrupt.size()),
        "forged image counts fail before allocation of their table");
  corrupt = *encoded;
  corrupt[0] ^= 1;
  CHECK(!DecodeGeometryAsset(corrupt, corrupt.size()), "unsupported format versions are misses");
  corrupt = *encoded;
  std::fill_n(corrupt.end() - 4, 4, uint8_t{255});
  CHECK(!DecodeGeometryAsset(corrupt, corrupt.size()),
        "native indices outside the vertex table are rejected after structural decoding");
  for (size_t end = 0; end < encoded->size(); end += 31) {
    CHECK(!DecodeGeometryAsset(std::span(*encoded).first(end), encoded->size()),
          "truncated native fields never publish a partial scene");
  }
  return Report();
}
