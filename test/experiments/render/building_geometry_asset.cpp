#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "BuildingGeometrySource.h"
#include "GeometryProductCodec.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ratio>
#include <string>
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
  using namespace outshine::Generators;
  if (argc < 8) {
    std::cerr << "building_geometry_asset <asset-db> <base-key> <output-db> <triangle-attempts> "
                 "<encoded-byte-limit> <source-count> <source-ordinal>...\n";
    return 2;
  }
  BuildingGeometrySource::Budget budget;
  if (!Number(argv[4], budget.TriangleAttempts) || !Number(argv[5], budget.EncodedBytesMost)) {
    return 2;
  }
  BuildingGeometrySource::Recipe recipe{.BaseKey = argv[2], .Sources = {}};
  if (!Number(argv[6], recipe.SourceCount)) { return 2; }
  for (int at = 7; at < argc; ++at) {
    uint32_t ordinal = 0;
    if (!Number(argv[at], ordinal)) { return 2; }
    recipe.Sources.push_back(ordinal);
  }
  auto output = AssetCache::Open(argv[3]);
  if (!output) { return 3; }
  Data::RawSourceCache raw({.Directory = std::string(argv[3]) + "-unused-sources"});
  const Data::SourceSet sources(raw);
  std::shared_ptr<PreparedBuildingAssets> assets;
  if (budget.TriangleAttempts != 0) {
    auto original = AssetCache::Open(argv[1]);
    if (!original) { return 3; }
    const std::shared_ptr<AssetCache> inputs(std::move(*original));
    auto opened = PreparedBuildingAssets::Open(inputs, sources);
    if (!opened) { return 4; }
    assets = std::move(*opened);
  }
  const auto began = std::chrono::steady_clock::now();
  auto product = BuildingGeometrySource::Resolve(**output, assets.get(), recipe, budget);
  if (!product) {
    std::cerr << product.error();
    return 6;
  }
  const auto builtMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto first = assets ? assets->Costs() : PreparedBuildingAssets::Counters{};
  auto fresh = AssetCache::Open(argv[3]);
  if (!fresh) { return 3; }
  const auto hitBegan = std::chrono::steady_clock::now();
  const auto hit = BuildingGeometrySource::Resolve(
      **fresh,
      nullptr,
      recipe,
      {.EncodedBytesMost = budget.EncodedBytesMost, .TriangleAttempts = 0});
  const auto hitMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - hitBegan)
          .count();
  const auto after = assets ? assets->Costs() : PreparedBuildingAssets::Counters{};
  if (!hit || !hit->Layout.Coverage || !product->Layout.Coverage ||
      hit->Layout.Coverage != product->Layout.Coverage ||
      EncodeGeometryProduct(hit->Mesh, budget.EncodedBytesMost) !=
          EncodeGeometryProduct(product->Mesh, budget.EncodedBytesMost) ||
      after.ReadBytes != first.ReadBytes || after.SurfaceReadBytes != first.SurfaceReadBytes) {
    return 7;
  }
  if (!hit->Layout.Coverage) { return 7; }
  const auto sourceCount = hit->Layout.Coverage->SourceCount();
  const auto record = (*fresh)->Find(recipe.Key());
  if (!record || !*record) { return 8; }
  size_t triangles = 0;
  for (int part = 0; part < hit->Mesh.parts(); ++part) {
    triangles += hit->Mesh.trianglesOf(part).size() / 3;
  }
  std::cout << R"({"input_owner":)" << (assets ? "true" : "false") << R"(,"sources":)"
            << recipe.Sources.size() << R"(,"parent_sources":)" << sourceCount << R"(,"triangles":)"
            << triangles << R"(,"encoded_bytes":)" << (**record).ByteCount
            << R"(,"first_header_bytes":)" << first.ReadBytes << R"(,"first_form_bytes":)"
            << first.SurfaceReadBytes << R"(,"build_ms":)" << builtMs
            << R"(,"fresh_source_free_hit_ms":)" << hitMs << R"(,"key":")" << recipe.Key()
            << "\"}\n";
  return 0;
}
}

int main(int argc, char *const *argv) {
  return Run(argc, argv);
}
