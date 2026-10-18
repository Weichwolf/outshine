#ifndef OUTSHINE_TEST_NATIVE_PLANT_WORLD_PREVIEW_H
#define OUTSHINE_TEST_NATIVE_PLANT_WORLD_PREVIEW_H

#include "NativePlantWorldAssets.h"
#include "NativeTreeDetailPreview.h"
#include "PlantLimbSurface.h"
#include "MaterialMaps.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::NativePlantWorldProbe {
inline std::filesystem::path Database;
inline std::string Identity;
constexpr float kBarkRed = 0.12f;
constexpr float kBarkGreen = 0.075f;
constexpr float kBarkBlue = 0.035f;
constexpr float kBarkRoughness = 0.85f;

inline Mat4 Placement(const Generators::PlantLeafPose &leaf) {
  Mat4 result;
  for (size_t column = 0; column < 3; ++column) {
    result.At(0, column) = leaf.Frame[column];
    result.At(1, column) = leaf.Frame[6 + column];
    result.At(2, column) = -leaf.Frame[3 + column];
  }
  result.At(0, 3) = leaf.OriginM[0];
  result.At(1, 3) = leaf.OriginM[2];
  result.At(2, 3) = -leaf.OriginM[1];
  return result;
}

inline std::expected<Geometry, std::string> Wood(const Source &source) {
  constexpr size_t kMostTriangles = 262144;
  constexpr size_t kMostVertices = 262144;
  const Generators::PlantLimbSurfaceDemand demand{
      .Along = 2, .Sides = 8, .MaximumVertices = kMostVertices, .MaximumTriangles = kMostTriangles};
  size_t vertices = 0;
  size_t triangles = 0;
  for (const auto &limb : source.Wood) {
    const auto size = Generators::MeasurePlantLimbSurface(limb, demand);
    if (!size || size->Vertices > kMostVertices - vertices ||
        size->Triangles > kMostTriangles - triangles) {
      return std::unexpected("demanded wood exceeds whole prototype capacity");
    }
    vertices += size->Vertices;
    triangles += size->Triangles;
  }
  Geometry::Mesh mesh;
  mesh.PositionsM.reserve(vertices * 3);
  mesh.Normals.reserve(vertices * 3);
  mesh.Indices.reserve(triangles * 3);
  for (const auto &limb : source.Wood) {
    auto surface = Generators::BuildPlantLimbSurface(limb, demand);
    if (!surface) { return std::unexpected(std::string(surface.error())); }
    const auto base = static_cast<uint32_t>(mesh.PositionsM.size() / 3);
    mesh.PositionsM.append_range(surface->Positions);
    mesh.Normals.append_range(surface->Normals);
    for (const auto index : surface->Triangles) { mesh.Indices.push_back(base + index); }
  }
  mesh.Uv.resize(vertices * 2, 0);
  Material material;
  material.BaseColour = {kBarkRed, kBarkGreen, kBarkBlue, 1};
  material.Roughness = kBarkRoughness;
  Geometry result;
  const auto slot = result.addSurface("wood", material);
  if (!slot) { return std::unexpected("wood material"); }
  const auto part = result.addPart("wood", *slot);
  if (!part || !result.adoptMesh(*part, mesh)) { return std::unexpected("wood product"); }
  return result;
}

inline std::expected<Geometry, std::string> Blade(const Geometry &source, size_t variant) {
  const int at = static_cast<int>(variant);
  Geometry result;
  auto material = source.surfaceAt(source.materialOf(at));
  std::vector<int> images(static_cast<size_t>(source.images()), -1);
  for (auto *map : MaterialMaps(material)) {
    if (!map->bound()) { continue; }
    if (map->Image >= source.images()) { return std::unexpected("leaf image reference"); }
    auto &copied = images[static_cast<size_t>(map->Image)];
    if (copied < 0) {
      const auto image = result.addImage(source.imageAt(map->Image));
      if (!image) { return std::unexpected("leaf image copy"); }
      copied = *image;
    }
    map->Image = copied;
  }
  const auto slot = result.addSurface("leaf", material);
  if (!slot) { return std::unexpected("leaf material"); }
  const auto part = result.addPart("leaf", *slot);
  if (!part || !result.setPositions(*part, source.positionsOf(at)) ||
      !result.setNormals(*part, source.normalsOf(at)) ||
      !result.setTangents(*part, source.tangentsOf(at)) ||
      !result.setTexture(*part, source.textureOf(at)) ||
      !result.setTriangles(*part, source.trianglesOf(at))) {
    return std::unexpected("leaf prototype");
  }
  return result;
}

inline bool FitsLeafDemand(const Source &source) {
  constexpr size_t kModelBytesMost = size_t{16} * 1024 * 1024;
  size_t count = 0;
  for (const auto host : source.Crown.Hosts()) {
    const auto measured = source.Crown.MeasureLeaves({.Node = host, .Maximum = 100000});
    if (!measured || *measured > kModelBytesMost / sizeof(Mat4) - count) { return false; }
    count += *measured;
  }
  return true;
}

inline bool Build(const Source &source, const Mat4 &root, double targetHeight, std::string &error) {
  if (!FitsLeafDemand(source)) {
    error = "selected plant exceeds explicit near-instance demand";
    return false;
  }
  double height = 0;
  for (const auto &point : source.Crown.Skeleton().PositionsM) {
    height = std::max(height, point[2]);
  }
  if (!(height > 0) || !(targetHeight > 0)) {
    error = "plant height";
    return false;
  }
  Mat4 scale;
  for (size_t axis = 0; axis < 3; ++axis) { scale.At(axis, axis) = targetHeight / height; }
  const Mat4 model = root * scale;
  Mat4 conversion;
  conversion.At(1, 1) = conversion.At(2, 2) = 0;
  conversion.At(1, 2) = 1;
  conversion.At(2, 1) = -1;
  const Mat4 woodModel = model * conversion;
  auto wood = Wood(source);
  if (!wood) {
    error = wood.error();
    return false;
  }
  if (!NativeTreeDetailProbe::Place(std::move(*wood), std::span(&woodModel, 1), error)) {
    return false;
  }
  std::array<std::vector<Mat4>, 8> rows;
  for (size_t variant = 0; variant < rows.size(); ++variant) {
    rows[variant].reserve(source.Crown.Summarize().Variants[variant]);
  }
  for (const auto host : source.Crown.Hosts()) {
    const auto leaves = source.Crown.Leaves({.Node = host, .Maximum = 100000});
    if (!leaves) {
      error = leaves.error();
      return false;
    }
    for (const auto &leaf : *leaves) {
      if (leaf.Variant >= rows.size()) {
        error = "unmatched physical variant";
        return false;
      }
      rows[leaf.Variant].push_back(model * Placement(leaf.Pose));
    }
  }
  for (size_t variant = 0; variant < rows.size(); ++variant) {
    if (rows[variant].empty()) { continue; }
    auto blade = Blade(source.Blades, variant);
    if (!blade) {
      error = blade.error();
      return false;
    }
    if (!NativeTreeDetailProbe::Place(std::move(*blade), rows[variant], error)) { return false; }
  }
  return true;
}

template <class Groups>
bool Prepare(Groups &groups, Render::SceneRenderer &renderer, std::string &error) {
  const auto began = std::chrono::steady_clock::now();
  const auto selected = NativeTreeDetailProbe::ReadSelection(groups);
  if (!selected || selected->Chosen.size() != 1) {
    error = "one original source tree required";
    return false;
  }
  const auto choice = selected->Chosen.front();
  auto cache = AssetCache::Open(Database.string());
  if (!cache) {
    error = "plant cache open";
    return false;
  }
  auto source = Load(**cache, Identity);
  const auto loaded = std::chrono::steady_clock::now();
  if (!source) {
    error = source.error();
    return false;
  }
  auto &held = NativeTreeDetailProbe::Held;
  held.Renderer = &renderer;
  held.Far.resize(groups.size());
  for (size_t group = 0; group < groups.size(); ++group) {
    for (size_t row = 0; row < groups[group].Models.size(); ++row) {
      if (group != choice.Group || row != choice.Row) {
        held.Far[group].push_back(groups[group].Models[row]);
      }
    }
    groups[group].ViewOrigin.reset();
  }
  if (!Build(*source, choice.Original, groups[choice.Group].Species->HeightM(), error)) {
    return false;
  }
  std::println(
      stderr,
      "PLANT trees=1 nodes={} hosts={} leaves={} references={} model_bytes={} load_ms={:.3f} "
      "build_ms={:.3f}",
      source->Crown.Summarize().Nodes,
      source->Crown.Hosts().size(),
      source->Crown.Summarize().Leaves,
      held.References,
      held.ModelBytes,
      std::chrono::duration<double, std::milli>(loaded - began).count(),
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loaded).count());
  return true;
}
}
#endif
