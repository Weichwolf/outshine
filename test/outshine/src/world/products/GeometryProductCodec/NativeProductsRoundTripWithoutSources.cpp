#include "GeometryProductCodec.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Test;

constexpr Material kOpticalParameters{.DiffuseTransmission = {},
                                      .Pattern = SurfacePattern::Facade,
                                      .BaseColour = {{0.2f, 0.3f, 0.4f, 0.5f}},
                                      .Metalness = 0.2f,
                                      .Roughness = 0.4f,
                                      .Transmission = 0.1f,
                                      .Ior = 1.4f,
                                      .Emission = {{0.2f, 0.1f, 0.3f}},
                                      .Alpha = AlphaMode::Masked,
                                      .DoubleSided = true,
                                      .CoverageCut = 0.25f,
                                      .Unlit = true,
                                      .NeedsTangents = true,
                                      .NormalScale = 0.3f,
                                      .OcclusionStrength = 0.6f,
                                      .SpecularFactor = 0.4f,
                                      .SpecularColour = {{0.7f, 0.8f, 0.9f}},
                                      .SheenColour = {{0.1f, 0.2f, 0.3f}},
                                      .SheenRoughness = 0.3f,
                                      .Clearcoat = 0.7f,
                                      .ClearcoatRoughness = 0.2f,
                                      .Anisotropy = 0.1f,
                                      .AnisotropyRotationRad = 0.3f,
                                      .Iridescence = 0.4f,
                                      .IridescenceIor = 1.5f,
                                      .IridescenceThicknessMinNm = 110.0f,
                                      .IridescenceThicknessMaxNm = 220.0f,
                                      .Thickness = 0.8f,
                                      .AttenuationDistance = 15.0f,
                                      .AttenuationColour = {{0.6f, 0.5f, 0.4f}},
                                      .BaseColourMap = {},
                                      .NormalMap = {},
                                      .MetalRoughMap = {},
                                      .EmissiveMap = {},
                                      .OcclusionMap = {},
                                      .SpecularStrengthMap = {},
                                      .SpecularTintMap = {},
                                      .DiffuseReflectionMap = {},
                                      .DiffuseTransmissionMap = {}};

Geometry Product() {
  Geometry geometry;
  const std::array<uint8_t, 16> rgba = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  const auto image = geometry.addImage(2, 2, rgba);
  CHECK(image && *image == 0, "native image uses the first owner-local slot");
  Material surface = kOpticalParameters;
  constexpr double mapOffsetStep = 0.1;
  constexpr double mapOffsetY = -0.2;
  constexpr double mapRotationStep = 0.2;
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
    map->Uv = {.OffsetUv = {{mapIndex * mapOffsetStep, mapOffsetY}},
               .RotationRad = mapIndex * mapRotationStep,
               .ScaleUv = {{2.0, -3.0}}};
    ++mapIndex;
  }
  const auto material = geometry.addSurface("all material features", surface);
  CHECK(material, "full native material is accepted");
  const auto defaultMaterial = geometry.addSurface("infinite attenuation", Material{});
  CHECK(defaultMaterial, "the native default permits infinite attenuation distance");
  Material thin;
  thin.Scattering = ScatteringModel::DiffuseThinSheet;
  constexpr Vec4f thinReflection{{0.1f, 0.2f, 0.03f, 1}};
  thin.BaseColour = thinReflection;
  constexpr Vec3f thinTransmission{{0.4f, 0.6f, 0.1f}};
  thin.DiffuseTransmission = thinTransmission;
  thin.DoubleSided = true;
  CHECK(geometry.addSurface("unlit optical coefficients", thin),
        "native products store direct energy-bounded thin-sheet R/T alongside opaque materials");
  Material mixture;
  mixture.Scattering = ScatteringModel::AreaMixture;
  mixture.Alpha = AlphaMode::Masked;
  mixture.DiffuseTransmission = {{1, 1, 1}};
  mixture.BaseColourMap = surface.BaseColourMap;
  mixture.DiffuseReflectionMap = surface.NormalMap;
  mixture.DiffuseTransmissionMap = surface.OcclusionMap;
  CHECK(geometry.addSurface("coverage-weighted optics", mixture),
        "native products carry mapped optical contributions and their independent transforms");
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
  constexpr PunctualLight light{.Kind = LightKind::Spot,
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
          "both scattering models, direct R/T, factors, maps, UV transforms and names survive "
          "exactly");
  }
  for (int index = 0; index < a.images(); ++index) {
    const auto before = a.imageAt(index);
    const auto after = b.imageAt(index);
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
  const auto &before = a.lampAt(0);
  const auto &after = b.lampAt(0);
  CHECK(before.Kind == after.Kind && before.Colour == after.Colour &&
            before.Intensity == after.Intensity && before.Position == after.Position &&
            before.Direction == after.Direction && before.InnerConeRad == after.InnerConeRad &&
            before.OuterConeRad == after.OuterConeRad && before.RangeM == after.RangeM &&
            a.lampNameOf(0) == b.lampNameOf(0) && a.lampPlacementOf(0) == b.lampPlacementOf(0),
        "native photometric light and placement survive");
}
}

int main() {
  const auto empty = EncodeGeometryProduct(Geometry{}, 24);
  const auto emptyProduct = empty ? DecodeGeometryProduct(*empty, 24) : std::nullopt;
  CHECK(emptyProduct && emptyProduct->parts() == 0 && emptyProduct->lamps() == 0,
        "a known empty native scene remains distinct from corruption or an absent asset");
  const Geometry original = Product();
  const auto encoded = EncodeGeometryProduct(original, 1u << 20u);
  CHECK(encoded, "a complete native product encodes within its byte bound");
  if (!encoded) { return Report(); }
  auto decoded = DecodeGeometryProduct(*encoded, encoded->size());
  CHECK(decoded, "the complete native product decodes without source data or a device");
  if (decoded) { Compare(original, *decoded); }
  CHECK(!EncodeGeometryProduct(original, encoded->size() - 1) &&
            !DecodeGeometryProduct(*encoded, encoded->size() - 1),
        "encoding and decoding respect the caller's exact payload bound");
  auto corrupt = *encoded;
  corrupt.push_back(0);
  CHECK(!DecodeGeometryProduct(corrupt, corrupt.size()), "trailing bytes are not a native product");
  corrupt = *encoded;
  std::fill_n(corrupt.begin() + 8, 4, std::numeric_limits<uint8_t>::max());
  CHECK(!DecodeGeometryProduct(corrupt, corrupt.size()),
        "forged image counts fail before allocation of their table");
  corrupt = *encoded;
  corrupt[0] ^= 1;
  CHECK(!DecodeGeometryProduct(corrupt, corrupt.size()), "unsupported format versions are misses");
  corrupt = *encoded;
  corrupt[5] = 3;
  CHECK(!DecodeGeometryProduct(corrupt, corrupt.size()),
        "pre-optical material assets are rejected instead of misreading their old layout");
  corrupt = *encoded;
  std::fill_n(corrupt.end() - 4, 4, std::numeric_limits<uint8_t>::max());
  CHECK(!DecodeGeometryProduct(corrupt, corrupt.size()),
        "native indices outside the vertex table are rejected after structural decoding");
  constexpr size_t truncationStride = 31;
  for (size_t end = 0; end < encoded->size(); end += truncationStride) {
    CHECK(!DecodeGeometryProduct(std::span(*encoded).first(end), encoded->size()),
          "truncated native fields never publish a partial scene");
  }
  return Report();
}
