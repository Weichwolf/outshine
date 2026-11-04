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
#include <limits>
#include <string>
#include <string_view>
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

bool WriteFrame(BinaryValueWriter &out, const Generators::BuildingCaptureFrame &frame) {
  const auto number = [&out](double value) { return out(value == 0 ? 0.0 : value); };
  for (const auto &axis :
       std::array{frame.Axes.East, frame.Axes.North, frame.Axes.Up, frame.Origin}) {
    for (const auto value : axis) {
      if (!number(value)) { return false; }
    }
  }
  return true;
}

bool WriteCapture(BinaryValueWriter &out, const BuildingImpostorSource::Recipe &recipe) {
  const auto number = [&out](double value) { return out(value == 0 ? 0.0 : value); };
  if (!WriteFrame(out, recipe.Frame)) { return false; }
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

std::string MakeKey(const BuildingImpostorSource::Recipe &recipe, bool geometry) {
  const auto &[BaseKey, Cells, Sources, Frame, Directions, WallRoofMetreMaterials, Pixels] = recipe;
  const bool group = !Sources.empty();
  constexpr size_t sourcesMost =
      std::min<size_t>(std::numeric_limits<uint32_t>::max(),
                       (std::numeric_limits<size_t>::max() - kRecipeBytesMost) / sizeof(uint32_t));
  if (!AssetProductKey::Parse(BaseKey) || Cells.empty() == Sources.empty() || Cells.size() > 64 ||
      Sources.size() > sourcesMost || !Frame.Valid() ||
      (!geometry &&
       !ImpostorAtlas::EncodedBytesMost({.Pixels = static_cast<int>(Pixels), .Views = 1}))) {
    return {};
  }
  const auto materials = Materials(WallRoofMetreMaterials);
  if (!materials) { return {}; }
  auto selected = group ? Sources : Cells;
  std::ranges::sort(selected);
  const auto duplicates = std::ranges::unique(selected);
  selected.erase(duplicates.begin(), duplicates.end());
  BinaryValueWriter out(kRecipeBytesMost + selected.size() * sizeof(uint32_t));
  const std::string_view viewFamily =
      group ? "building-source-group-view-1" : "building-complete-original-view-4";
  const std::string_view family = geometry ? "building-source-mesh-1" : viewFamily;
  if (!out.Text(family) || !out.Text(BaseKey) ||
      (geometry ? !out(static_cast<uint8_t>(group)) : !out(Pixels)) ||
      !out(static_cast<uint32_t>(selected.size()))) {
    return {};
  }
  for (const auto identity : selected) {
    if ((!group && (identity == 0 || identity > 64)) || !out(identity)) { return {}; }
  }
  if (!(geometry ? WriteFrame(out, Frame) : WriteCapture(out, recipe)) ||
      !out(static_cast<uint64_t>(materials->size())) || !out.Out.Put(*materials)) {
    return {};
  }
  const auto bytes = out.Out.Bytes();
  return Sha256Hex(bytes.data(), bytes.size());
}
}

std::string BuildingImpostorSource::Recipe::Key() const {
  return MakeKey(*this, false);
}

std::string BuildingImpostorSource::Recipe::GeometryKey() const {
  return MakeKey(*this, true);
}
}
