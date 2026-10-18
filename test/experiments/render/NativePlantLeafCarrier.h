#ifndef OUTSHINE_TEST_NATIVE_PLANT_LEAF_CARRIER_H
#define OUTSHINE_TEST_NATIVE_PLANT_LEAF_CARRIER_H

#include "scene/Geometry.h"
#include "BinaryValueArchive.h"
#include "ImageMips.h"
#include "LeafOpticsDefaults.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine::NativePlantWorldProbe {
inline Material LeafMaterial() {
  constexpr float kLeafRoughness = 0.8f;
  const auto rgb = Generators::DefaultLeafReflection();
  Material material;
  material.BaseColour = {rgb[0], rgb[1], rgb[2], 1};
  material.Roughness = kLeafRoughness;
  material.DoubleSided = true;
  material.Scattering = ScatteringModel::DiffuseThinSheet;
  material.DiffuseTransmission = Generators::DefaultLeafTransmission();
  return material;
}

inline bool CarrierArray(BinaryValueReader &in, auto &values, size_t count) {
  uint32_t actual = 0;
  if (!in(actual) || actual != count) { return false; }
  values.resize(count);
  return std::ranges::all_of(values, [&](auto &value) { return in(value); });
}

inline std::expected<int, std::string>
CarrierImage(BinaryValueReader &in, Geometry &geometry, uint32_t size, ImageMipKind kind) {
  const auto bytes = in.In.Take(static_cast<size_t>(size) * size * 4);
  if (!bytes) { return std::unexpected("carrier image bytes"); }
  ImageView image{
      .WidthPx = static_cast<int>(size), .HeightPx = static_cast<int>(size), .Rgba = *bytes};
  ImageMipData mips;
  mips[static_cast<size_t>(kind)] = Core::PrepareImageMips(image, kind);
  if (!mips[static_cast<size_t>(kind)]) { return std::unexpected("carrier mip preparation"); }
  image.LowerMips = ViewImageMips(mips);
  const auto added = geometry.addImage(image);
  if (!added) { return std::unexpected("carrier image publication"); }
  return *added;
}

inline bool CarrierPart(BinaryValueReader &in, Geometry &geometry, uint32_t size, bool baseLevel) {
  constexpr uint32_t kCoarseCoordinates = 27;
  constexpr uint32_t kFineCoordinates = 45;
  uint32_t floats = 0;
  if (!in(floats) || (floats != kCoarseCoordinates && floats != kFineCoordinates)) { return false; }
  const size_t vertices = floats / 3;
  Geometry::Mesh mesh;
  mesh.PositionsM.resize(floats);
  if (!std::ranges::all_of(mesh.PositionsM, [&](auto &value) { return in(value); }) ||
      !CarrierArray(in, mesh.Uv, vertices * 2) ||
      !CarrierArray(in, mesh.Indices, (vertices / 3 - 1) * 12)) {
    return false;
  }
  for (size_t at = 0; at < vertices; ++at) {
    mesh.Normals.insert(mesh.Normals.end(), {0, 0, 1});
    mesh.Tangents.insert(mesh.Tangents.end(), {1, 0, 0, 1});
  }
  const auto colour = CarrierImage(in, geometry, size, ImageMipKind::Colour);
  const auto normal = CarrierImage(in, geometry, size, ImageMipKind::Normal);
  if (!colour || !normal) { return false; }
  auto material = LeafMaterial();
  material.Alpha = AlphaMode::Masked;
  material.BaseColourMap.Image = *colour;
  material.NormalMap.Image = *normal;
  if (baseLevel) { material.BaseColourMap.Sampler.Mip = MipFilter::None; }
  for (auto *map : {&material.BaseColourMap, &material.NormalMap}) {
    map->Sampler.WrapU = map->Sampler.WrapV = Wrap::ClampToEdge;
  }
  const auto name = std::to_string(geometry.parts());
  const auto surface = geometry.addSurface(name, material);
  if (!surface) { return false; }
  const auto part = geometry.addPart(name, *surface);
  return part && geometry.adoptMesh(*part, mesh).has_value();
}

inline std::expected<Geometry, std::string> ReadCarrierPacket(const std::string &path,
                                                              bool baseLevel) {
  constexpr size_t kMostPacketBytes = size_t{1024} * 1024;
  constexpr uint64_t kFormat = 0x31524643464cULL;
  std::error_code error;
  const auto length = std::filesystem::file_size(path, error);
  if (error || length > kMostPacketBytes) { return std::unexpected("carrier input size"); }
  std::vector<uint8_t> bytes(length);
  std::ifstream input(path, std::ios::binary);
  if (!input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(length))) {
    return std::unexpected("carrier input read");
  }
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  uint32_t size = 0;
  uint32_t variants = 0;
  if (!in(format, size, variants) || format != kFormat || size != 64 || variants != 8) {
    return std::unexpected("carrier input header");
  }
  Geometry geometry;
  for (size_t variant = 0; variant < variants; ++variant) {
    if (!CarrierPart(in, geometry, size, baseLevel)) { return std::unexpected("carrier part"); }
  }
  if (in.In.Remaining() != 0) { return std::unexpected("trailing carrier input"); }
  return geometry;
}
}
#endif
