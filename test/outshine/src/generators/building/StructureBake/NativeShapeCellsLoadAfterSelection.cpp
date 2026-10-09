#include "Check.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructurePlan.h"
#include "BuildingSurfaceBlock.h"
#include "BuildingScratch.h"
#include "BuildingMesh.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "content/AssetCache.h"
#include "Geodesy.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  const Ground::GeoBounds region{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  for (uint32_t index = 0; index < 3; ++index) {
    const double lon = 9.001 + static_cast<double>(index) * 0.004;
    const double length = index == 0 ? 0.001 : 0.0002;
    const double width = index == 0 ? 0.00004 : 0.0002;
    const std::array ring{
        47.001, lon, 47.001, lon + length, 47.001 + width, lon + length, 47.001 + width, lon};
    const auto cell = StructureCellOf(region, ring);
    CHECK(cell.has_value(), "fixture belongs to a spatial cell");
    if (!cell) { return {}; }
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back(
        {.LocalFirst = first,
         .PointCount = 4,
         .SourceFirst = first,
         .Cell = *cell,
         .HeightM = 12,
         .Pitched = 0,
         .Facade = index == 2 ? std::optional(FacadeStyle::Tower) : std::nullopt});
  }
  return raw;
}

std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {125, 125, 125, 125};
  return Ground::HeightField::Of(0, {block});
}

void MissingCellIsGenerated(const std::filesystem::path &root,
                            const std::string &key,
                            PreparedBuildingAssets &cache) {
  auto database = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(database.has_value(), "native index is independently queryable");
  if (!database) { return; }
  const auto parent = (*database)->Find(key);
  CHECK(parent && *parent, "parent metadata has spatial bounds");
  if (!parent || !*parent) { return; }
  const auto child = (*database)->Select({.Bounds = (**parent).Bounds, .Kind = "building-forms"});
  CHECK(child.has_value() && child->size() == 3, "each occupied native cell has its own package");
  if (!child || child->empty()) { return; }
  const auto erased = (*database)->Remove(child->front().Key);
  CHECK(erased.has_value(), "fixture removes one form cell without removing its parent");
  const auto metadata = cache.Load(key);
  CHECK(metadata && *metadata, "complete plan/contact parent survives a missing shape child");
  if (!metadata || !*metadata) { return; }
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  const auto built = BakePreparedStructures(**metadata, view, mesher, *scratch);
  CHECK(built && cache.Costs().SurfaceMisses == 1,
        "needed missing cell is a generator miss, not missing visible geometry");
}

void OriginalStorageIsReused(const std::filesystem::path &root,
                             const std::string &currentKey,
                             const PreparedStructureTile &reference,
                             PreparedBuildingAssets &cache) {
  auto database = AssetCache::Open((root / "assets.sqlite").string());
  const auto encoded = EncodePreparedStructureTile(reference);
  CHECK(database && encoded, "original native format remains readable and writable");
  if (!database || !encoded) { return; }
  const auto current = (*database)->Find(currentKey);
  CHECK(current && *current, "published parent supplies conservative spatial bounds");
  if (!current || !*current) { return; }
  const auto key = cache.Key({.Zoom = 0, .X = 0, .Y = 0}, 17, {}, 1000);
  const AssetRecord record{.Key = key,
                           .Kind = "buildings",
                           .Bounds = (**current).Bounds,
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Parent = {}};
  const auto published = (*database)->Publish(std::span(&record, 1), *encoded);
  CHECK(published.has_value(), "fixture publishes a fully enriched original native asset");
  if (!published) { return; }
  const auto before = cache.Costs();
  const auto metadata = cache.Load(key);
  CHECK(metadata && *metadata && cache.Costs().SurfaceReadBytes == before.SurfaceReadBytes,
        "original native hit upgrades storage without reading child shapes or using providers");
  if (!metadata || !*metadata) { return; }
  const auto stored = (*database)->Load(key, encoded->size() * 2);
  CHECK(stored && *stored &&
            DecodePreparedStructureIndex((**stored).Bytes(), size_t{1} * 1024 * 1024),
        "same logical key now names the atomically published new index format");
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  auto referenceScratch = mesher.Scratch();
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  view.RequestedCell = reference.Structures.front().Layout.Cell.Index;
  const auto built = BakePreparedStructures(**metadata, view, mesher, *scratch);
  const auto expected = BakePreparedStructures(reference, view, mesher, *referenceScratch);
  CHECK(built && expected && built->Digest == expected->Digest && built->Prints == expected->Prints,
        "upgraded enriched asset keeps its geometry and terrain contacts");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-building-cells-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(root.string(), sources);
  CHECK(cache.has_value(), "native building cache opens");
  if (!cache) { return Report(); }
  auto raw = Inputs();
  auto heights = Heights();
  const auto reference = PrepareStructureTile(raw, *heights);
  CHECK(reference.has_value(), "cold native data supplies the reference");
  if (reference) {
    const auto full = EncodePreparedStructureTile(*reference);
    const auto decoded =
        full ? DecodePreparedStructureTile(*full, size_t{1} * 1024 * 1024) : std::nullopt;
    CHECK(decoded && decoded->Structures[2].Layout.Facade == FacadeStyle::Tower &&
              !decoded->Structures[0].Layout.Facade &&
              decoded->Surfaces[2].Shapes().front().OpeningStyle == FacadeStyle::Tower,
          "native restart retains supplied service use and independent opening plans");
    auto invalid = *reference;
    invalid.Structures[2].Layout.Facade = static_cast<FacadeStyle>(8);
    CHECK(!EncodePreparedStructureTile(invalid), "invalid opening families cannot publish");
    CHECK(reference->Surfaces.front().FaceCount() > 4 * 8 + 16,
          "long terrace yields more faces than a source-point multiplier can bound");
    auto index = EncodePreparedStructureIndex(*reference);
    const auto decodedIndex =
        index ? DecodePreparedStructureIndex(*index, size_t{1} * 1024 * 1024) : std::nullopt;
    CHECK(decodedIndex && decodedIndex->Structures[2].Layout.Facade == FacadeStyle::Tower,
          "spatial metadata retains opening use before shape reads");
    CHECK(index && index->size() >= 5, "fixture encodes native selection metadata");
    if (index && index->size() >= 5) {
      std::fill_n(index->end() - 5, 4, uint8_t{255});
      CHECK(!DecodePreparedStructureIndex(*index, size_t{1} * 1024 * 1024),
            "untrusted face counts cannot exceed the decoder's resident memory budget");
    }
  }
  const auto key = (*cache)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000);
  const std::atomic_bool stop{false};
  auto base = (*cache)->Generate(key, raw, *heights, stop);
  CHECK(base && *base && (*cache)->Costs().SurfaceReadBytes == 0,
        "parent generation reloads metadata without reading any form cell");
  if (!reference || !base || !*base) { return Report(); }
  raw = {};
  heights.reset();
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  const auto &one = (*base)->Structures.front();
  auto plan = PreparedStructurePlan(
      one,
      (*base)->PointsLatLon,
      (*base)->Holes,
      std::span((*base)->CornerAslM).subspan(one.CornerFirst, one.Layout.PointCount),
      (*base)->AnchorEcef);
  plan.Prepared = &(*base)->Surfaces.front();
  CHECK(mesher.SourceEnvelopeBounds(plan, *scratch) && mesher.ShellSurfaceErrorM(plan, *scratch) &&
            (*cache)->Costs().SurfaceReadBytes == 0,
        "bounds and LOD error queries do not open fine form data");
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  view.RequestedCell = one.Layout.Cell.Index;
  const auto selected = BakePreparedStructures(**base, view, mesher, *scratch);
  auto referenceScratch = mesher.Scratch();
  const auto expected = BakePreparedStructures(*reference, view, mesher, *referenceScratch);
  CHECK(selected && expected && selected->Digest == expected->Digest &&
            selected->Prints == expected->Prints && (*cache)->Costs().SurfaceHits == 1,
        "selected cell reproduces cold geometry while other occupied cells stay unopened");
  CHECK((*cache)->Costs().SurfaceReadBytes > 0 && (*cache)->Costs().SurfaceMisses == 0,
        "loaded form is a native hit after source and height owners disappear");
  base->reset();
  MissingCellIsGenerated(root, key, **cache);
  OriginalStorageIsReused(root, key, *reference, **cache);
  cache->reset();
  std::filesystem::remove_all(root);
  return Report();
}
