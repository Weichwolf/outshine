#include "GeometryAsset.h"
#include "GeometryProductCodec.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytes = 4096;

Geometry Triangle() {
  Geometry geometry;
  const auto surface = geometry.addSurface("concrete", Material{});
  CHECK(surface, "the generic asset has a physical material");
  if (!surface) { return geometry; }
  const auto part = geometry.addPart("wall", *surface);
  CHECK(part, "the generic asset has one renderable part");
  if (!part) { return geometry; }
  CHECK(geometry.setPositions(*part, std::array<float, 9>{0, 0, 0, 3, 0, 0, 0, 3, 0}) &&
            geometry.setTexture(*part, std::array<float, 6>{0, 0, 1, 0, 0, 1}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "native geometry is ready before cache publication");
  return geometry;
}

void CheckCoverage(AssetCache &cache,
                   GeometryAsset::Description layout,
                   const Geometry &mesh,
                   const std::string &path) {
  const std::string key(64, 'e');
  const std::string parent(64, 'f');
  std::vector<SourceCoverage::Range> ranges;
  ranges.reserve(50);
  for (uint64_t at = 0; at < 50; ++at) { ranges.push_back({.First = at * 3, .Count = 1}); }
  auto coverage = SourceCoverage::FromRanges(parent, 1000000000000ULL, ranges);
  CHECK(coverage, "sparse product ownership is valid independently of its mesh");
  if (!coverage) { return; }
  layout.Coverage = std::move(*coverage);
  const auto encoded = EncodeGeometryProduct(mesh, kBytes);
  if (!encoded) { return; }
  CHECK(!GeometryAsset::Publish(cache, key, parent, layout, mesh, encoded->size()) &&
            cache.Find(key) && !*cache.Find(key),
        "description admission fails before mesh publication when only the mesh would fit");
  CHECK(GeometryAsset::Publish(cache, key, parent, layout, mesh, kBytes),
        "coverage and native geometry publish within the explicit per-record budget");
  auto fresh = AssetCache::Open(path);
  CHECK(fresh, "the coverage owner can be reopened without generator state");
  if (!fresh) { return; }
  const auto metadata = GeometryAsset::Describe(**fresh, key, parent, kBytes);
  const auto loaded = GeometryAsset::Load(**fresh, key, parent, kBytes);
  CHECK(metadata && *metadata && (**metadata).Coverage == layout.Coverage && loaded && *loaded &&
            (**loaded).Layout.Coverage == layout.Coverage,
        "revision, large domain and exact sparse intervals survive fresh-owner restore");
  CHECK(!GeometryAsset::Describe(**fresh, key, parent, 184),
        "metadata reads obey the caller budget before decoding dynamic ranges");
  CHECK((*fresh)->Remove(key), "mesh eviction can precede metadata inspection");
  const auto isolated = GeometryAsset::Describe(**fresh, key, parent, kBytes);
  CHECK(isolated && *isolated && (**isolated).Coverage == layout.Coverage,
        "coverage metadata restores without reading any mesh or prepared-source payload");
  const auto rows = (*fresh)->Select({.Bounds = layout.Bounds.Through(layout.WorldFromLocal),
                                      .Kind = "native-geometry-info",
                                      .Level = std::nullopt,
                                      .Radius = std::nullopt,
                                      .Planes = {},
                                      .Parent = parent});
  CHECK(rows && rows->size() == 1, "the index finds the separate product descriptor");
  if (!rows || rows->size() != 1) { return; }
  auto record = rows->front();
  std::vector<uint8_t> corrupt;
  {
    const auto original = (*fresh)->Load(record.Key, kBytes);
    if (!original || !*original) { return; }
    const auto bytes = (**original).Bytes();
    corrupt.assign(bytes.begin(), bytes.end());
  }
  record.Package.clear();
  record.OffsetBytes = 0;
  std::fill(corrupt.end() - 8, corrupt.end(), uint8_t{0});
  CHECK((*fresh)->Remove(record.Key) && (*fresh)->Publish(std::span(&record, 1), corrupt),
        "the fixture stores an otherwise valid descriptor with an invalid final source range");
  CHECK(!GeometryAsset::Describe(**fresh, key, parent, kBytes),
        "corrupt ownership cannot restore as an unqualified legacy descriptor");
}

void Exercise(const std::filesystem::path &root) {
  const auto path = (root / "assets.sqlite").string();
  auto opened = AssetCache::Open(path);
  CHECK(opened, "a generic asset store opens");
  if (!opened) { return; }
  const std::string key(64, 'a');
  const std::string parent(64, 'b');
  GeometryAsset::Description layout{.Bounds = {.Min = {{0, 0, 0}}, .Max = {{3, 3, 0}}},
                                    .WorldFromLocal = {}};
  layout.WorldFromLocal.SetTranslation({{10, 20, 30}});
  const auto missing = GeometryAsset::Load(**opened, key, parent, kBytes);
  CHECK(missing && !*missing, "a missing native product stays a cache miss");
  auto geometry = Triangle();
  const auto encoded = EncodeGeometryProduct(geometry, kBytes);
  CHECK(encoded, "the native mesh has a stable product representation");
  if (!encoded) { return; }
  CHECK(!GeometryAsset::Publish(**opened, key, parent, layout, geometry, 1),
        "an insufficient encoded-product budget fails before publication");
  CHECK(GeometryAsset::Publish(**opened, key, parent, layout, geometry, kBytes),
        "complete geometry and small metadata are published");
  geometry.clear();
  opened->reset();
  auto fresh = AssetCache::Open(path);
  if (!fresh) {
    CHECK(false, "the fresh process-equivalent asset owner opens");
    return;
  }
  const auto metadata = GeometryAsset::Describe(**fresh, key, parent, kBytes);
  CHECK(metadata && *metadata && (**metadata).Bounds == layout.Bounds &&
            (**metadata).WorldFromLocal == layout.WorldFromLocal,
        "source-free metadata preserves local bounds and the world transform");
  const auto loaded = GeometryAsset::Load(**fresh, key, parent, kBytes);
  CHECK(loaded && *loaded && EncodeGeometryProduct((**loaded).Mesh, kBytes) == encoded,
        "a fresh owner gets the same renderable geometry without a producer");
  if (!loaded || !*loaded) { return; }
  const auto &mesh = (**loaded).Mesh;
  CHECK(GeometryAsset::Publish(**fresh, std::string(64, 'd'), "", layout, mesh, kBytes),
        "generic assets also support roots without an external source provider");
  CHECK(!GeometryAsset::Load(**fresh, key, std::string(64, 'c'), kBytes) &&
            !GeometryAsset::Describe(**fresh, "invalid", parent, kBytes),
        "foreign source revisions and malformed addresses are rejected");
  auto bad = layout;
  bad.Bounds.Max[0] = 1;
  CHECK(!GeometryAsset::Publish(**fresh, std::string(64, 'd'), parent, bad, mesh, kBytes),
        "published spatial metadata cannot exclude native geometry");
  bad = layout;
  bad.WorldFromLocal.At(3, 0) = 1;
  CHECK(!GeometryAsset::Publish(**fresh, std::string(64, 'd'), parent, bad, mesh, kBytes),
        "perspective matrices are not accepted as asset placements");
  bad = layout;
  bad.WorldFromLocal[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!GeometryAsset::Publish(**fresh, std::string(64, 'd'), parent, bad, mesh, kBytes),
        "nonfinite placements cannot reach the spatial index");
  const auto worldBounds = layout.Bounds.Through(layout.WorldFromLocal);
  const auto rows = (*fresh)->Select({.Bounds = worldBounds,
                                      .Kind = "native-geometry-info",
                                      .Level = std::nullopt,
                                      .Radius = std::nullopt,
                                      .Planes = {},
                                      .Parent = parent});
  CHECK(rows && rows->size() == 1 && rows->front().ByteCount == 8 + 22 * 8,
        "spatial selection finds one fixed-size descriptor without reading its mesh");
  if (!rows || rows->size() != 1) { return; }
  CHECK((*fresh)->Remove(key), "the mesh can be evicted independently");
  const auto infoOnly = GeometryAsset::Describe(**fresh, key, parent, kBytes);
  const auto evicted = GeometryAsset::Load(**fresh, key, parent, kBytes);
  CHECK(infoOnly && *infoOnly && evicted && !*evicted,
        "metadata survives eviction but is not reported as a resident mesh product");
  auto record = rows->front();
  CHECK((*fresh)->Remove(record.Key), "the prior descriptor is removed for a corruption case");
  std::vector<uint8_t> truncated{0};
  record.Package.clear();
  record.OffsetBytes = 0;
  record.ByteCount = truncated.size();
  CHECK((*fresh)->Publish(std::span(&record, 1), truncated), "the malformed descriptor is stored");
  CHECK(!GeometryAsset::Describe(**fresh, key, parent, kBytes),
        "a truncated descriptor fails rather than becoming an implicit cache miss");
  CheckCoverage(**fresh, layout, mesh, path);
}
}

int main() {
  const auto root = outshine::Test::PlantedPath("geometry-native-assets");
  Exercise(root);
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
