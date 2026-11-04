#include "AssetCache.h"
#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "ImpostorBaker.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "TileGeodesy.h"
#include <SDL3/SDL.h>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ratio>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
template <class T> bool Number(std::string_view text, T &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

int Run(int argc, char *const *argv) {
  using namespace outshine;
  if (argc < 6) {
    std::cerr << "building_geometry_cache <asset-db> <base-key> <output-db> <geometry-byte-limit> "
                 "<source-ordinal>...\n";
    return 2;
  }
  size_t geometryBytes = 0;
  if (!Number(argv[4], geometryBytes)) { return 2; }
  auto original = AssetCache::Open(argv[1]);
  auto database = AssetCache::Open(argv[3]);
  if (!original || !database) { return 3; }
  const std::shared_ptr<AssetCache> inputs(std::move(*original));
  const std::shared_ptr<AssetCache> cache(std::move(*database));
  Data::RawSourceCache raw({.Directory = std::string(argv[3]) + "-unused-sources"});
  const Data::SourceSet sources(raw);
  const auto assets = Generators::PreparedBuildingAssets::Open(inputs, sources);
  if (!assets) { return 4; }
  const auto base = (*assets)->Load(argv[2]);
  if (!base || !*base) { return 5; }
  BuildingImpostorSource::Recipe recipe;
  recipe.BaseKey = argv[2];
  uint64_t faces = 0;
  for (int at = 5; at < argc; ++at) {
    uint32_t ordinal = 0;
    if (!Number(argv[at], ordinal) || ordinal >= (*base)->Surfaces.size()) { return 2; }
    recipe.Sources.push_back(ordinal);
    faces += (*base)->Surfaces[ordinal].FaceCount();
  }
  const auto &anchor = (*base)->AnchorEcef;
  const auto geo = Ground::EcefToGeoWgs84({.X = anchor[0], .Y = anchor[1], .Z = anchor[2]});
  recipe.Frame = {
      .Axes = EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}),
      .Origin = {}};
  recipe.Directions = {{{0, 0, -1}}};
  recipe.WallRoofMetreMaterials = Generators::BuildingSurfaceMaterials();
  recipe.Pixels = 64;
  const auto fieldBytes = ImpostorAtlas::EncodedBytesMost({.Pixels = 64, .Views = 1});
  if (!fieldBytes || faces == 0) { return 6; }
  const BuildingImpostorSource::Budget budget{
      .FieldBytesMost = *fieldBytes,
      .GeometryBytesMost = geometryBytes,
      .Capture = {.Samples = uint64_t{64} * 64,
                  .Sources = recipe.Sources.size(),
                  .Faces = faces,
                  .TriangleAttempts = geometryBytes / (3 * sizeof(uint32_t))}};
  Render::ImpostorBaker captures;
  const auto before = (*assets)->Costs();
  const auto began = std::chrono::steady_clock::now();
  BuildingImpostorSource producer(recipe, *assets);
  const auto metadata = producer.ResolveMetadata(*cache);
  const auto first = producer.Generate(captures, *cache, {0}, budget);
  if (!metadata || !first) {
    std::cerr << (first ? metadata.error() : first.error());
    return 7;
  }
  const auto generatedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto once = (*assets)->Costs();
  const auto record = cache->Find(recipe.GeometryKey());
  recipe.Pixels = 32;
  recipe.Directions = {{{1, 0, 0}}};
  const auto started = std::chrono::steady_clock::now();
  BuildingImpostorSource offline(recipe, nullptr);
  const auto offlineMetadata = offline.ResolveMetadata(*cache);
  const auto second = offline.Generate(captures, *cache, {0}, budget);
  if (!offlineMetadata || !second) {
    std::cerr << (second ? offlineMetadata.error() : second.error());
    return 8;
  }
  const auto hitMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  const auto after = (*assets)->Costs();
  if (!record || !*record || after.ReadBytes != once.ReadBytes ||
      after.SurfaceReadBytes != once.SurfaceReadBytes) {
    return 9;
  }
  std::cout << "{\"sources\":" << recipe.Sources.size() << ",\"faces\":" << faces
            << ",\"geometry_bytes\":" << (**record).ByteCount
            << ",\"first_header_bytes\":" << once.ReadBytes - before.ReadBytes
            << ",\"first_form_bytes\":" << once.SurfaceReadBytes - before.SurfaceReadBytes
            << ",\"first_metadata_mesh_capture_ms\":" << generatedMs
            << ",\"source_free_metadata_load_capture_ms\":" << hitMs << R"(,"geometry_key":")"
            << recipe.GeometryKey() << "\"}\n";
  return 0;
}
}

int main(int argc, char *const *argv) {
  if (!SDL_Init(SDL_INIT_VIDEO)) { return 10; }
  const int result = Run(argc, argv);
  SDL_Quit();
  return result;
}
