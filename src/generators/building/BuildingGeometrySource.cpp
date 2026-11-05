#include "BuildingGeometrySource.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "BuildingMesh.h"
#include "BuildingMeshConversion.h"
#include "BuildingScratch.h"
#include "BuildingSourceView.h"
#include "BuildingMaterials.h"
#include "AssetPublication.h"
#include "BinaryValueArchive.h"
#include "GeometryProductCodec.h"
#include "TileGeodesy.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
std::expected<Geometry, std::string> Materials() {
  Geometry result;
  for (const auto &material : BuildingSurfaceMaterials()) {
    if (!result.addSurface("building-group", material)) {
      return std::unexpected("invalid building group material");
    }
  }
  return result;
}

std::vector<uint32_t> Canonical(const BuildingGeometrySource::Recipe &recipe) {
  auto sources = recipe.Sources;
  std::ranges::sort(sources);
  return sources;
}

bool Matches(const GeometryAsset::Description &layout,
             const BuildingGeometrySource::Recipe &recipe,
             std::span<const uint32_t> selected) {
  if (!layout.Coverage) { return false; }
  const auto expected = SourceCoverage::FromSources(recipe.BaseKey, recipe.SourceCount, selected);
  return expected && *expected == *layout.Coverage;
}

std::expected<GeometryAsset::Product, std::string> Convert(BakedStructureGroup group,
                                                           const std::string &revision) {
  const auto geo = Ground::EcefToGeoWgs84(
      {.X = group.AnchorEcef[0], .Y = group.AnchorEcef[1], .Z = group.AnchorEcef[2]});
  const BuildingCaptureFrame frame{
      .Axes = EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}),
      .Origin = {}};
  auto geometry = Materials();
  if (!geometry) { return std::unexpected(geometry.error()); }
  Box bounds;
  const auto &raised = group.Mesh;
  const std::array parts{BuildingMeshPart{.Vertices = raised.WallCorners,
                                          .Indices = raised.WallRun,
                                          .Colours = raised.WallColours,
                                          .MaterialM = raised.WallMaterialM},
                         BuildingMeshPart{.Vertices = raised.RoofCorners,
                                          .Indices = raised.RoofRun,
                                          .Colours = {},
                                          .MaterialM = {}}};
  for (size_t at = 0; at < parts.size(); ++at) {
    if (parts[at].Vertices.empty()) { continue; }
    auto mesh = ConvertBuildingMesh(parts[at], frame.Axes);
    if (!mesh) { return std::unexpected(std::string(mesh.error())); }
    for (size_t vertex = 0; vertex < mesh->PositionsM.size(); vertex += 3) {
      bounds.Cover(Vec3{
          {mesh->PositionsM[vertex], mesh->PositionsM[vertex + 1], mesh->PositionsM[vertex + 2]}});
    }
    const size_t material = at == 0 && !parts[at].MaterialM.empty() ? 2 : at;
    const auto part =
        geometry->addPart("building-group", MaterialInstance(static_cast<int>(material)));
    if (!part || !geometry->adoptMesh(*part, *mesh)) {
      return std::unexpected("invalid native building group geometry");
    }
  }
  auto coverage = SourceCoverage::FromSources(revision, group.SourceCount, group.Sources);
  if (!coverage || bounds.Empty()) { return std::unexpected("incomplete native building group"); }
  Mat4 world;
  for (size_t axis = 0; axis < 3; ++axis) {
    Vec3 unit;
    unit[axis] = 1;
    const auto direction = frame.SourceDirection(unit);
    for (size_t row = 0; row < 3; ++row) { world.At(row, axis) = direction[row]; }
  }
  world.SetTranslation(group.AnchorEcef);
  return GeometryAsset::Product{
      .Layout = {.Bounds = bounds, .WorldFromLocal = world, .Coverage = std::move(*coverage)},
      .Mesh = std::move(*geometry),
      .Clusters = {}};
}
}

std::string BuildingGeometrySource::Recipe::Key() const {
  if ((Detail != LevelOfDetail::Fine && Detail != LevelOfDetail::Shell) ||
      !AssetProductKey::Parse(BaseKey) || SourceCount == 0 || Sources.empty() ||
      Sources.size() > std::numeric_limits<uint32_t>::max()) {
    return {};
  }
  const auto sources = Canonical(*this);
  if (sources.back() >= SourceCount || std::ranges::adjacent_find(sources) != sources.end()) {
    return {};
  }
  const auto materials = Materials();
  if (!materials) { return {}; }
  const auto encoded = EncodeGeometryProduct(*materials, std::numeric_limits<size_t>::max());
  if (!encoded) { return {}; }
  BinaryValueWriter out(std::numeric_limits<size_t>::max());
  const char *const family = Detail == LevelOfDetail::Fine ? "building-native-fine-group-1"
                                                           : "building-native-shell-group-1";
  if (!out.Text(family) || !out.Text(BaseKey) || !out(SourceCount) ||
      !out.Array<uint32_t>(sources) || !out.Out.Put(*encoded)) {
    return {};
  }
  const auto bytes = out.Out.Bytes();
  return Sha256Hex(bytes.data(), bytes.size());
}

std::expected<GeometryAsset::Product, std::string>
BuildingGeometrySource::Resolve(AssetCache &cache,
                                PreparedBuildingAssets *sources,
                                const Recipe &recipe,
                                Budget budget,
                                const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("building group request cancelled"); }
  const auto key = recipe.Key();
  if (key.empty() || budget.EncodedBytesMost == 0) {
    return std::unexpected("invalid building group request or admission");
  }
  const auto selected = Canonical(recipe);
  auto cached = GeometryAsset::Load(cache, key, recipe.BaseKey, budget.EncodedBytesMost);
  if (!cached) { return std::unexpected("native building group lookup failed"); }
  if (*cached) {
    if (!Matches((**cached).Layout, recipe, selected)) {
      return std::unexpected("native building group source coverage differs from its recipe");
    }
    if (stop.stop_requested()) { return std::unexpected("building group load cancelled"); }
    return std::move(**cached);
  }
  if (budget.TriangleAttempts == 0) {
    return std::unexpected("native building group work not admitted");
  }
  if (sources == nullptr) { return std::unexpected("native building group inputs unavailable"); }
  const auto base = sources->Load(recipe.BaseKey);
  if (!base || !*base || (*base)->Structures.size() != recipe.SourceCount) {
    return std::unexpected("prepared building group inputs unavailable or foreign source domain");
  }
  std::atomic_bool cancelled(stop.stop_requested());
  const std::stop_callback cancellation(stop,
                                        [&] { cancelled.store(true, std::memory_order_relaxed); });
  BuildingScratch scratch;
  scratch.Triangles.emplace(BuildingScratch::TriangleBudget{.Remaining = budget.TriangleAttempts});
  RawTile view;
  view.RequestedDetail = recipe.Detail;
  const BuildingMesh mesher;
  auto baked = BakePreparedStructureSources(**base, view, selected, mesher, scratch, &cancelled);
  if (!baked) {
    if (scratch.Triangles && scratch.Triangles->Exhausted) {
      return std::unexpected("native building geometry exhausted its triangle-work allowance");
    }
    return std::unexpected(std::string(Describe(baked.error())));
  }
  auto product = Convert(std::move(*baked), recipe.BaseKey);
  if (!product) { return product; }
  if (stop.stop_requested()) { return std::unexpected("building group generation cancelled"); }
  const auto stored = GeometryAsset::Publish(
      cache, key, recipe.BaseKey, product->Layout, product->Mesh, budget.EncodedBytesMost);
  if (!stored) { return std::unexpected("native building group publication failed"); }
  return product;
}
}
