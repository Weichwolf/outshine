#include "BuildingImpostorSource.h"
#include "ImpostorCaptureMaterial.h"
#include "AssetPublication.h"
#include "BinaryValueArchive.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kMaterialBytesMost = 16384, kRecipeBytesMost = 32768;
constexpr size_t kCatalogueViewsMost = 64;
constexpr double kDirectionNormTolerance = 1e-9;

std::optional<std::vector<uint8_t>> Materials(const std::array<Material, 3> &rows) {
  Geometry geometry;
  std::vector<Material> captured;
  captured.reserve(rows.size());
  for (const auto &row : rows) {
    const auto output = CapturedImpostorMaterial(row);
    if (!output || row.Alpha != AlphaMode::Opaque || row.Unlit ||
        row.Scattering != ScatteringModel::MetallicRoughness || row.BaseColourMap.bound() ||
        row.NormalMap.bound() || row.MetalRoughMap.bound() ||
        !geometry.addSurface("building-source", row)) {
      return std::nullopt;
    }
    captured.push_back(*output);
  }
  std::string error;
  ImpostorAtlas::View blank{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  blank.Texels.resize(9);
  if (!ImpostorAtlas::Create(3, {}, 1, std::move(captured), {std::move(blank)}, error)) {
    return std::nullopt;
  }
  return EncodeGeometryProduct(geometry, kMaterialBytesMost);
}

bool WriteCapture(BinaryValueWriter &out, const BuildingImpostorSource::Recipe &recipe) {
  const auto number = [&out](double value) { return out(value == 0 ? 0.0 : value); };
  const auto &frame = recipe.Frame;
  for (const auto &axis :
       std::array{frame.Axes.East, frame.Axes.North, frame.Axes.Up, frame.Origin}) {
    for (const auto value : axis) {
      if (!number(value)) { return false; }
    }
  }
  if (recipe.Directions.empty() || recipe.Directions.size() > kCatalogueViewsMost ||
      !out(static_cast<uint32_t>(recipe.Directions.size()))) {
    return false;
  }
  for (const auto &direction : recipe.Directions) {
    if (!std::ranges::all_of(direction, [](double value) { return std::isfinite(value); }) ||
        std::abs(Dot(direction, direction) - 1) > kDirectionNormTolerance) {
      return false;
    }
    for (const auto value : direction) {
      if (!number(value)) { return false; }
    }
  }
  return true;
}
}

std::string BuildingImpostorSource::Recipe::Key() const {
  if (!AssetProductKey::Parse(BaseKey) || Cells.empty() || Cells.size() > 64 || !Frame.Valid() ||
      !ImpostorAtlas::EncodedBytesMost({.Pixels = static_cast<int>(Pixels), .Views = 1})) {
    return {};
  }
  const auto materials = Materials(WallRoofMetreMaterials);
  if (!materials) { return {}; }
  auto cells = Cells;
  std::ranges::sort(cells);
  const auto duplicates = std::ranges::unique(cells);
  cells.erase(duplicates.begin(), duplicates.end());
  BinaryValueWriter out(kRecipeBytesMost);
  if (!out.Text("building-complete-original-view-3") || !out.Text(BaseKey) ||
      !out(Pixels, static_cast<uint32_t>(cells.size()))) {
    return {};
  }
  for (const auto cell : cells) {
    if (cell == 0 || cell > 64 || !out(cell)) { return {}; }
  }
  if (!WriteCapture(out, *this) || !out(static_cast<uint64_t>(materials->size())) ||
      !out.Out.Put(*materials)) {
    return {};
  }
  const auto bytes = out.Out.Bytes();
  return Sha256Hex(bytes.data(), bytes.size());
}
}
