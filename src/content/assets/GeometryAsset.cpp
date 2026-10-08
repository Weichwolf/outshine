#include "content/GeometryAsset.h"
#include "BinaryValueArchive.h"
#include "GeometryAssetValues.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <limits>
#include <utility>

namespace outshine {
namespace {
constexpr uint64_t kGeometryAssetFormat = 0x0001314f45474fULL;
constexpr uint64_t kPreparedGeometryAssetFormat = 0x0002314f45474fULL;
static_assert(sizeof(float) == 4 && sizeof(double) == 8 && std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<double>::is_iec559);

struct TableCounts {
  int Images = 0;
  int Surfaces = 0;
  int Lamps = 0;
  int Parts = 0;

  [[nodiscard]] bool Fits(size_t remaining) const noexcept {
    if (Images < 0 || Surfaces < 0 || Lamps < 0 || Parts < 0) { return false; }
    const uint64_t records = static_cast<uint64_t>(Images) + Surfaces + Lamps + Parts;
    return records <= remaining / sizeof(uint32_t);
  }
};

bool ValidLamp(const PunctualLight &light, const Mat4 &placement) {
  return static_cast<uint32_t>(light.Kind) <= static_cast<uint32_t>(LightKind::Spot) &&
         std::ranges::all_of(placement.Column, [](double one) { return std::isfinite(one); });
}

bool Complete(const Geometry &geometry) {
  return geometry.wellFormed() || (geometry.parts() == 0 && geometry.surfaces() == 0 &&
                                   geometry.images() == 0 && geometry.lamps() == 0);
}

bool WriteImages(BinaryValueWriter &out, const Geometry &geometry, bool preparedImages) {
  for (int index = 0; index < geometry.images(); ++index) {
    const ImageView image = geometry.imageAt(index);
    if (!out(image.WidthPx, image.HeightPx) || !out.Array(image.Rgba)) { return false; }
    if (preparedImages) {
      uint8_t present = 0;
      for (size_t kind = 0; kind < image.LowerMips.size(); ++kind) {
        if (image.LowerMips[kind]) { present |= static_cast<uint8_t>(1u << kind); }
      }
      if (!out(present)) { return false; }
      for (const auto &levels : image.LowerMips) {
        if (levels && !out.Array(*levels)) { return false; }
      }
    }
  }
  return true;
}

bool WriteTables(BinaryValueWriter &out, const Geometry &geometry) {
  for (int index = 0; index < geometry.surfaces(); ++index) {
    Material surface = geometry.surfaceAt(MaterialInstance(index));
    if (!out.Text(geometry.surfaceNameOf(index)) || !Content::GeometryAssetMaterial(out, surface)) {
      return false;
    }
  }
  for (int index = 0; index < geometry.lamps(); ++index) {
    PunctualLight light = geometry.lampAt(index);
    if (!ValidLamp(light, geometry.lampPlacementOf(index)) ||
        !out.Text(geometry.lampNameOf(index)) || !Content::GeometryAssetLight(out, light) ||
        !out(geometry.lampPlacementOf(index).Column)) {
      return false;
    }
  }
  return true;
}

bool WriteParts(BinaryValueWriter &out, const Geometry &geometry) {
  for (int part = 0; part < geometry.parts(); ++part) {
    if (!out.Text(geometry.nameOf(part)) ||
        !out(geometry.materialOf(part).index(), geometry.placementOf(part).Column) ||
        !out.Array(geometry.positionsOf(part)) || !out.Array(geometry.normalsOf(part)) ||
        !out.Array(geometry.textureOf(part)) || !out.Array(geometry.textureOf(part, UvSet::Uv1)) ||
        !out.Array(geometry.tangentsOf(part)) || !out.Array(geometry.coloursOf(part)) ||
        !out.Array(geometry.trianglesOf(part))) {
      return false;
    }
  }
  return true;
}

bool ReadImages(BinaryValueReader &in, Geometry &geometry, int count, bool preparedImages) {
  for (int index = 0; index < count; ++index) {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
    if (!in(width, height) || !in.Array(pixels)) { return false; }
    ImageMipData mips;
    if (preparedImages) {
      uint8_t present = 0;
      if (!in(present) || present >= (1u << mips.size())) { return false; }
      for (size_t kind = 0; kind < mips.size(); ++kind) {
        if ((present & (1u << kind)) != 0) {
          auto &levels = mips[kind].emplace();
          if (!in.Array(levels)) { return false; }
        }
      }
    }
    if (!geometry.addImage({.WidthPx = width,
                            .HeightPx = height,
                            .Rgba = pixels,
                            .LowerMips = ViewImageMips(mips)})) {
      return false;
    }
  }
  return true;
}

bool ReadTables(BinaryValueReader &in, Geometry &geometry, const TableCounts &tables) {
  std::string name;
  for (int index = 0; index < tables.Surfaces; ++index) {
    Material surface;
    if (!in.Text(name) || !Content::GeometryAssetMaterial(in, surface) ||
        !geometry.addSurface(name, surface)) {
      return false;
    }
  }
  for (int index = 0; index < tables.Lamps; ++index) {
    PunctualLight light;
    Mat4 placement;
    if (!in.Text(name) || !Content::GeometryAssetLight(in, light) || !in(placement.Column) ||
        !ValidLamp(light, placement) || !geometry.addLamp(name, light, placement)) {
      return false;
    }
  }
  return true;
}

bool ReadParts(BinaryValueReader &in, Geometry &geometry, int parts) {
  std::string name;
  std::vector<float> attribute;
  std::vector<uint32_t> indices;
  for (int index = 0; index < parts; ++index) {
    int surface = -1;
    Mat4 placement;
    if (!in.Text(name) || !in(surface, placement.Column)) { return false; }
    const auto part = geometry.addPart(name, MaterialInstance(surface));
    if (!part || !geometry.setPlacement(*part, placement) || !in.Array(attribute) ||
        !geometry.setPositions(*part, attribute) || !in.Array(attribute) ||
        !geometry.setNormals(*part, attribute) || !in.Array(attribute) ||
        !geometry.setTexture(*part, attribute) || !in.Array(attribute) ||
        !geometry.setTexture(*part, attribute, 1) || !in.Array(attribute) ||
        !geometry.setTangents(*part, attribute) || !in.Array(attribute) ||
        !geometry.setColours(*part, attribute) || !in.Array(indices) ||
        !geometry.setTriangles(*part, indices)) {
      return false;
    }
  }
  return true;
}
}

std::optional<std::vector<uint8_t>> EncodeGeometryAsset(const Geometry &geometry,
                                                        size_t bytesMost) {
  if (!Complete(geometry)) { return std::nullopt; }
  BinaryValueWriter out(bytesMost);
  out.Out.Reserve(geometry.storageBytes());
  bool preparedImages = false;
  for (int index = 0; index < geometry.images(); ++index) {
    const auto image = geometry.imageAt(index);
    preparedImages = preparedImages || std::ranges::any_of(image.LowerMips, [](const auto &levels) {
                       return levels.has_value();
                     });
  }
  if (!out(preparedImages ? kPreparedGeometryAssetFormat : kGeometryAssetFormat,
           geometry.images(),
           geometry.surfaces(),
           geometry.lamps(),
           geometry.parts()) ||
      !WriteImages(out, geometry, preparedImages) || !WriteTables(out, geometry) ||
      !WriteParts(out, geometry)) {
    return std::nullopt;
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<Geometry> DecodeGeometryAsset(std::span<const uint8_t> bytes, size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::nullopt; }
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  TableCounts tables;
  if (!in(format, tables.Images, tables.Surfaces, tables.Lamps, tables.Parts) ||
      (format != kGeometryAssetFormat && format != kPreparedGeometryAssetFormat) ||
      !tables.Fits(in.In.Remaining())) {
    return std::nullopt;
  }
  Geometry geometry;
  if (!ReadImages(in, geometry, tables.Images, format == kPreparedGeometryAssetFormat) ||
      !ReadTables(in, geometry, tables) || !ReadParts(in, geometry, tables.Parts) ||
      in.In.Remaining() != 0 || !Complete(geometry)) {
    return std::nullopt;
  }
  return geometry;
}
}
