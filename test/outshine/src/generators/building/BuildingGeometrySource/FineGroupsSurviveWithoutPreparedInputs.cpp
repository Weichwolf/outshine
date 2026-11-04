#include "BuildingGeometrySource.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingAssetFixture.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "GeometryProductCodec.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <limits>
#include <stop_token>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBytes = size_t{1024} * 1024;
constexpr BuildingGeometrySource::Budget kBudget{.EncodedBytesMost = kBytes,
                                                 .TriangleAttempts = 100000};

size_t Triangles(const Geometry &geometry) {
  size_t total = 0;
  for (int part = 0; part < geometry.parts(); ++part) {
    total += geometry.trianglesOf(part).size() / 3;
  }
  return total;
}

void CheckFrame(const GeometryAsset::Product &product, const BakedStructureGroup &reference) {
  CHECK(!reference.Mesh.WallCorners.empty(), "the reference has a wall vertex");
  if (reference.Mesh.WallCorners.empty()) { return; }
  const auto positions = product.Mesh.positionsOf(0);
  CHECK(positions.size() >= 3, "the product has its corresponding local vertex");
  if (positions.size() < 3) { return; }
  const Vec3 local{{positions[0], positions[1], positions[2]}};
  const auto &point = reference.Mesh.WallCorners.front().pos;
  const Vec3 delta{{point[0], point[1], point[2]}};
  const auto restored = product.Layout.WorldFromLocal.TransformPoint(local);
  const double bound = 8 * std::numeric_limits<float>::epsilon() * (Length(delta) + 1);
  CHECK(Length(restored - reference.AnchorEcef - delta) <= bound,
        "the local asset frame restores native ECEF positions within float rounding");
}

void CheckKeys(const std::string &key) {
  const BuildingGeometrySource::Recipe recipe{.BaseKey = key, .SourceCount = 2, .Sources = {0, 1}};
  const BuildingGeometrySource::Recipe reordered{
      .BaseKey = key, .SourceCount = 2, .Sources = {1, 0}};
  const BuildingGeometrySource::Recipe duplicate{
      .BaseKey = key, .SourceCount = 2, .Sources = {0, 0}};
  const BuildingGeometrySource::Recipe other{
      .BaseKey = std::string(64, 'b'), .SourceCount = 2, .Sources = {0, 1}};
  CHECK(!recipe.Key().empty() && reordered.Key() == recipe.Key() && duplicate.Key().empty() &&
            other.Key() != recipe.Key(),
        "group identity is canonical, revision bound and unambiguous");
}

void Exercise(const std::filesystem::path &root) {
  const auto cache = OpenAssetStore((root / "products").string());
  Data::RawSourceCache rawSource({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(rawSource);
  auto assets = PreparedBuildingAssets::Open(OpenAssetStore((root / "prepared").string()), sources);
  CHECK(assets, "the prepared source owner opens independently of render products");
  if (!assets) { return; }
  auto raw = BuildingAssetFixture::Inputs();
  const auto first = raw.Structures.front();
  raw.Structures.push_back(first);
  raw.Structures.back().HeightM += 5;
  const auto heights = BuildingAssetFixture::Heights();
  CHECK(heights, "the source fixture has heights");
  if (!heights) { return; }
  const std::atomic_bool stop{false};
  const auto key = (*assets)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000, std::string(64, 'a'));
  const auto base = (*assets)->Generate(key, raw, *heights, stop);
  CHECK(base && *base, "two source buildings prepare in one source cell");
  if (!base || !*base) { return; }
  CheckKeys(key);
  const BuildingGeometrySource::Recipe recipe{.BaseKey = key, .SourceCount = 2, .Sources = {0}};
  auto denied = kBudget;
  denied.TriangleAttempts = 1;
  CHECK(!BuildingGeometrySource::Resolve(*cache, assets->get(), recipe, denied),
        "a partial native mesh cannot be published when its work allowance is exhausted");
  const auto absent = cache->Find(recipe.Key());
  CHECK(absent && !*absent, "a failed group leaves no render asset behind");
  auto product = BuildingGeometrySource::Resolve(*cache, assets->get(), recipe, kBudget);
  CHECK(product && product->Layout.Coverage && product->Layout.Coverage->SourceCount() == 2,
        "the render group retains its complete immutable source domain");
  if (!product || !product->Layout.Coverage) { return; }
  const auto coverage = SourceCoverage::FromSources(key, 2, recipe.Sources);
  CHECK(coverage && *product->Layout.Coverage == *coverage,
        "one selected source does not claim its unselected neighbour");
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Fine;
  const BuildingMesh mesher;
  BuildingScratch scratch;
  scratch.Triangles.emplace(BuildingScratch::TriangleBudget{.Remaining = kBudget.TriangleAttempts});
  const auto reference =
      BakePreparedStructureSources(**base, view, recipe.Sources, mesher, scratch);
  CHECK(reference && Triangles(product->Mesh) ==
                         (reference->Mesh.WallRun.size() + reference->Mesh.RoofRun.size()) / 3,
        "the cached mesh retains the selected fine bake, not a camera-dependent cell or shell");
  if (reference) { CheckFrame(*product, *reference); }
  const auto encoded = EncodeGeometryProduct(product->Mesh, kBytes);
  const auto fresh = OpenAssetStore((root / "products").string());
  const auto before = (*assets)->Costs();
  const auto hit = BuildingGeometrySource::Resolve(
      *fresh, nullptr, recipe, {.EncodedBytesMost = kBytes, .TriangleAttempts = 0});
  CHECK(hit && EncodeGeometryProduct(hit->Mesh, kBytes) == encoded &&
            hit->Layout.Coverage == product->Layout.Coverage &&
            hit->Layout.WorldFromLocal == product->Layout.WorldFromLocal,
        "a fresh hit restores materials, geometry, frame and exact coverage without its producer");
  const auto after = (*assets)->Costs();
  CHECK(after.ReadBytes == before.ReadBytes && after.SurfaceReadBytes == before.SurfaceReadBytes,
        "the hit reads no prepared headers or forms");
  CHECK(!BuildingGeometrySource::Resolve(
            *fresh, nullptr, recipe, {.EncodedBytesMost = 1, .TriangleAttempts = 0}),
        "encoded residency limits apply to hits as well");
  std::stop_source cancelled;
  cancelled.request_stop();
  CHECK(!BuildingGeometrySource::Resolve(*fresh, nullptr, recipe, kBudget, cancelled.get_token()),
        "cancelled hits cannot acquire a product");
  const BuildingGeometrySource::Recipe other{.BaseKey = key, .SourceCount = 2, .Sources = {1}};
  CHECK(GeometryAsset::Publish(*fresh, other.Key(), key, product->Layout, product->Mesh, kBytes),
        "an intentionally misbound counterexample is stored");
  CHECK(!BuildingGeometrySource::Resolve(*fresh, assets->get(), other, kBudget),
        "a recipe never accepts another group's valid but incorrect source coverage");
  auto domain = recipe;
  ++domain.SourceCount;
  CHECK(
      domain.Key() != recipe.Key() &&
          GeometryAsset::Publish(*fresh, domain.Key(), key, product->Layout, product->Mesh, kBytes),
      "a changed source domain has its own recipe identity");
  CHECK(!BuildingGeometrySource::Resolve(*fresh, nullptr, domain, kBudget),
        "identical ordinals in a foreign source domain cannot satisfy the recipe");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-tests.fine-building-group-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Exercise(root);
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
