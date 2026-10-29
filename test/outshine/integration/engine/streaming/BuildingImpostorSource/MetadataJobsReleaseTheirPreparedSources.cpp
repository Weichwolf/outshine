#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Heap.h"
#include "HeightField.h"
#include "Check.h"
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Test;

size_t Release(std::unique_ptr<BuildingImpostorSource> &source) {
  const auto before = Heap::LiveBytes();
  source.reset();
  return before - Heap::LiveBytes();
}

void Run(const std::filesystem::path &root) {
  Data::RawSourceCache sources({.Directory = (root / "sources").string()});
  const Data::SourceSet sourceSet(sources);
  const auto opened =
      Generators::PreparedBuildingAssets::Open((root / "native").string(), sourceSet);
  CHECK(opened.has_value(), "native prepared source storage opens");
  auto cache = AssetCache::Open((root / "fields.sqlite").string());
  CHECK(cache.has_value(), "the field asset cache opens");
  if (!opened || !cache) { return; }
  const auto &assets = *opened;
  Generators::RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  raw.LatLon = {48.001, 16.001, 48.001, 16.0012, 48.0012, 16.0012, 48.0012, 16.001};
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 0,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 30,
                            .Pitched = 0,
                            .WallColour = {},
                            .Facade = {}});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "coarse native height inputs exist");
  if (!heights) { return; }
  const std::string digest(64, 'a');
  const auto key = assets->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000, digest);
  {
    const std::atomic_bool stop{false};
    const auto prepared = assets->Generate(key, raw, *heights, stop);
    CHECK(prepared && *prepared, "the prepared asset exists independently of the producer");
    if (!prepared || !*prepared) { return; }
  }
  const BuildingImpostorSource::Recipe recipe{
      .BaseKey = key,
      .Cells = {1},
      .Frame = {.Axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48}), .Origin = {}},
      .Directions = {{{0, 0, -1}}},
      .WallRoofMetreMaterials = Generators::BuildingSurfaceMaterials(),
      .Pixels = 32};
  auto control = std::make_unique<BuildingImpostorSource>(recipe, assets);
  const auto recipeBytes = Release(control);
  auto source = std::make_unique<BuildingImpostorSource>(recipe, assets);
  const auto before = assets->Costs();
  const auto metadata = source->ResolveMetadata(**cache);
  CHECK(metadata.has_value(), "the real metadata miss publishes a source-free description");
  CHECK(assets->Costs().ReadBytes > before.ReadBytes &&
            assets->Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "metadata reads prepared headers and never decodes source forms");
  const auto queriedBytes = Release(source);
  std::cout << "source_retention recipe_bytes=" << recipeBytes << " queried_bytes=" << queriedBytes
            << '\n';
  CHECK(Heap::ProcessInstrumentationEnabled() && recipeBytes > 0,
        "source lifetime compares actual nonempty instrumented ownership");
  CHECK(queriedBytes == recipeBytes,
        "a finished metadata job leaves only its recipe in the retained producer");
}
}

int main() {
  const std::filesystem::path root = outshine::Test::PlantedPath("building-metadata-lifetime");
  Run(root);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return outshine::Test::Report();
}
