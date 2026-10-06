#include "PreparedStructureTile.h"
#include "StructureBuildTask.h"
#include "Tasks.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructurePlan.h"
#include "AssetGeneration.h"
#include "Sha256.h"
#include "BuildingScratch.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <chrono>
#include <string>
#include <memory>
#include <vector>

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
    const double lon = 9.001 + static_cast<double>(index) * 0.002;
    const std::array ring{47.001, lon, 47.001, lon + 0.0002, 47.0012, lon + 0.0002, 47.0012, lon};
    const auto cell = StructureCellOf(region, ring);
    CHECK(cell.has_value(), "prepared fixture has a native cell");
    if (!cell) { return {}; }
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back({.LocalFirst = first,
                              .PointCount = 4,
                              .SourceFirst = first,
                              .Cell = *cell,
                              .HeightM = index == 0 ? 0.0 : 12.0,
                              .MinimumHeightM = index == 2 ? 3.0 : 0.0,
                              .Pitched = 0});
  }
  raw.Ways.push_back({.LocalFirst = static_cast<uint32_t>(raw.LatLon.size() / 2),
                      .PointCount = 2,
                      .HalfWidthM = 3.0f});
  raw.LatLon.insert(raw.LatLon.end(), {47.00096, 9.0, 47.00096, 9.02});
  return raw;
}

std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {125.125f, 131.25f, 142.5f, 149.75f};
  return Ground::HeightField::Of(0, {block});
}

std::optional<PreparedStructureTile> CachedBase(const PreparedStructureTile &base) {
  const auto encoded = EncodePreparedStructureTile(base);
  CHECK(encoded.has_value(), "complete intrinsic forms, roofs and terrain contacts serialize");
  if (!encoded) { return std::nullopt; }
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-prepared-building-cache-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "assets.sqlite").string();
  auto opened = AssetCache::Open(path);
  CHECK(opened.has_value(), "building asset fixture opens the common spatial cache");
  if (!opened) { return std::nullopt; }
  constexpr std::string_view recipe = "prepared-buildings-format-1";
  const auto key = Sha256Hex(recipe.data(), recipe.size());
  size_t generated = 0;
  const auto published = ResolveAsset(**opened, key, encoded->size(), [&](size_t) {
    ++generated;
    GeneratedAssetPackage package;
    package.Bytes = *encoded;
    package.Records.push_back({.Key = key,
                               .Kind = "buildings",
                               .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                               .ByteCount = package.Bytes.size()});
    return std::expected<GeneratedAssetPackage, std::string>(std::move(package));
  });
  CHECK(published && generated == 1, "one miss publishes an actual native prepared building tile");
  opened->reset();
  auto restarted = AssetCache::Open(path);
  CHECK(restarted.has_value(), "fresh cache connection opens the prepared building package");
  if (!restarted) { return std::nullopt; }
  const auto hit = ResolveAsset(**restarted, key, encoded->size(), [&](size_t) {
    ++generated;
    return std::expected<GeneratedAssetPackage, std::string>(std::unexpected("source offline"));
  });
  CHECK(hit && generated == 1, "native package hits bypass the unavailable generator and provider");
  if (!hit) { return std::nullopt; }
  const auto decoded = DecodePreparedStructureTile(hit->Bytes(), 1024 * 1024);
  CHECK(decoded.has_value(), "held cache bytes reconstruct complete source-independent shapes");
  if (!decoded) { return std::nullopt; }
  CHECK(!DecodePreparedStructureTile(hit->Bytes(), 1),
        "native arrays cannot allocate beyond the supplied resident budget");
  auto truncated = *encoded;
  truncated.pop_back();
  CHECK(!DecodePreparedStructureTile(truncated, 1024 * 1024),
        "partial native building packages do not become ready products");
  (*restarted).reset();
  std::filesystem::remove_all(root);
  return decoded;
}

void WorkerBake(const PreparedStructureTile &base,
                const BuildingMesh &mesher,
                const RawTile &view,
                uint64_t reference) {
  Tasks worker(1);
  auto held = std::make_shared<const PreparedStructureTile>(base);
  StructureBuildTask task(0,
                          held,
                          std::make_unique<RawTile>(view),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher.Scratch());
  held.reset();
  task.Start(worker, mesher);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  size_t tasks = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!task.TakeCompletion(worker)) {
      (void)task.AwaitCompletion(0.05);
      continue;
    }
    ++tasks;
    CHECK(task.Result().LastRanges > 0 &&
              task.Result().LastRanges <= StructureBuildTask::RangesPerTask,
          "ready base selection and emission keep the worker task bounded");
    if (!task.Result().Status || task.Result().Tile) { break; }
    task.Resume(worker, mesher);
  }
  if (task.Running()) {
    task.RequestStop();
    task.Join(worker);
  }
  CHECK(task.Result().Status && task.Result().Tile && task.Result().Tile->Digest == reference,
        "source-free worker delivers the same geometry from a held native cache basis");
  CHECK(base.Structures.size() <=
                StructureBuildTask::StructuresPerRange * StructureBuildTask::RangesPerTask ||
            tasks > 1,
        "a large ready tile yields the worker instead of emitting everything in one task");
  CHECK(task.Progress().BakedStructures() == base.Structures.size(),
        "native worker completion counts prepared structures without source progress");
}

void ReadyForms(const PreparedStructureTile &base, const BuildingMesh &mesher) {
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    const auto &record = base.Structures[index];
    const auto corners =
        std::span(base.CornerAslM).subspan(record.CornerFirst, record.Layout.PointCount);
    auto plan =
        PreparedStructurePlan(record, base.PointsLatLon, base.Holes, corners, base.AnchorEcef);
    plan.Prepared = &base.Surfaces[index];
    for (auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
      plan.Coarseness = detail;
      BuildingScratch scratch;
      Raised mesh;
      CHECK(mesher.SourceEnvelopeBounds(plan, scratch) &&
                mesher.ShellSurfaceErrorM(plan, scratch) && mesher.Mesh(plan, scratch, mesh),
            "prepared native forms supply bounds, error and every mesh envelope");
      CHECK(scratch.Parts.Count() == 0, "ready house forms never repeat intrinsic shape planning");
    }
  }
}

}

int main() {
  auto raw = Inputs();
  auto heights = Heights();
  const auto base = PrepareStructureTile(raw, *heights);
  CHECK(base && base->Structures.size() == 3 && base->CornerAslM.size() == 12 &&
            base->Surfaces.size() == 3,
        "one cold pass resolves every source footprint and terrain corner");
  if (!base) { return Report(); }
  CHECK(base->Structures[0].Standing.HeightM > 0 &&
            base->Structures[0].Standing.Source == Ground::BuildingHeightSource::Generated &&
            base->Structures[0].Standing.Street.Known,
        "unknown height and road frontage are fully enriched before runtime selection");
  CHECK(base->Structures[2].Standing.MinimumHeightM == 3.0f,
        "prepared raised parts retain their clearance");
  BuildingMesh mesher;
  std::vector<RawTile> views;
  std::vector<BakedTile> references;
  for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    RawTile view;
    view.RequestedDetail = detail;
    view.Eye = {.LongitudeDeg = 10, .LatitudeDeg = 47};
    views.push_back(view);
    auto original = raw;
    original.RequestedDetail = view.RequestedDetail;
    original.Eye = view.Eye;
    auto scratch = mesher.Scratch();
    BakedTile reference;
    CHECK(BakeStructures(original, *heights, mesher, *scratch, reference).has_value(),
          "cold native path supplies the geometry reference at every envelope");
    references.push_back(std::move(reference));
  }
  const auto decoded = CachedBase(*base);
  if (!decoded) { return Report(); }
  raw = {};
  heights.reset();
  ReadyForms(*decoded, mesher);
  WorkerBake(*decoded, mesher, views.front(), references.front().Digest);
  auto batch = *decoded;
  batch.Structures.resize(257, decoded->Structures.front());
  batch.Surfaces.resize(257, decoded->Surfaces.front());
  auto batchScratch = mesher.Scratch();
  const auto batchReference = BakePreparedStructures(batch, views.front(), mesher, *batchScratch);
  CHECK(batchReference.has_value(), "large native tile supplies the bounded worker fixture");
  if (batchReference) { WorkerBake(batch, mesher, views.front(), batchReference->Digest); }
  for (size_t index = 0; index < views.size(); ++index) {
    auto scratch = mesher.Scratch();
    const auto built = BakePreparedStructures(*decoded, views[index], mesher, *scratch);
    CHECK(built && built->Digest == references[index].Digest &&
              built->Prints == references[index].Prints && built->NoGround == 0,
          "prepared bases reproduce cold geometry and contacts after source/height owners vanish");
    CHECK(built && built->Coordinates && built->Coordinates->Points == decoded->PointsLatLon,
          "runtime native footprints carry their own geometry rather than source-cache views");
  }
  std::atomic_bool stopping{true};
  auto scratch = mesher.Scratch();
  const auto cancelled = BakePreparedStructures(*base, views[0], mesher, *scratch, &stopping);
  CHECK(!cancelled && cancelled.error() == StructureBakeError(StructureBakeErrorKind::Cancelled),
        "prepared native generation still obeys cancellation");
  return Report();
}
