#include "GeometryAsset.h"
#include "GeometryProductCodec.h"
#include "Check.h"
#include <array>
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
  const auto metadata = GeometryAsset::Describe(**fresh, key, parent);
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
            !GeometryAsset::Describe(**fresh, "invalid", parent),
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
  const auto infoOnly = GeometryAsset::Describe(**fresh, key, parent);
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
  CHECK(!GeometryAsset::Describe(**fresh, key, parent),
        "a truncated descriptor fails rather than becoming an implicit cache miss");
}
}

int main() {
  const auto root = outshine::Test::PlantedPath("geometry-native-assets");
  Exercise(root);
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
