#include "StructureBuildTask.h"
#include "StructureArtifact.h"
#include "BuildingMesh.h"
#include "Check.h"
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
class CountingMesh final : public outshine::Generators::BuildingMesh {
public:
  mutable std::atomic<size_t> Calls{0};

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &plan,
       outshine::MeshScratch &scratch,
       outshine::Raised &into) const noexcept override {
    Calls.fetch_add(1);
    return BuildingMesh::Mesh(plan, scratch, into);
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto pattern =
      (std::filesystem::temp_directory_path() / "outshine-structure-cache-XXXXXX").string();
  const char *created = mkdtemp(pattern.data());
  CHECK(created != nullptr, "temporary artifact directory exists");
  if (!created) { return Report(); }
  const std::filesystem::path root(created);
  RawTile raw;
  raw.LatLon = {47.0, 9.0, 47.0, 9.0001, 47.0001, 9.0001, 47.0001, 9.0};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9.0, .MinLatDeg = 47.0, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008};
  const auto cell = StructureCellOf(bounds, raw.LatLon);
  CHECK(cell.has_value(), "fixture cell exists");
  if (!cell) {
    std::filesystem::remove_all(root);
    return Report();
  }
  raw.Structures.push_back({.PointCount = 4, .Cell = *cell, .HeightM = 12.0});
  raw.RequestedDetail = LevelOfDetail::Shell;
  raw.RequestedCell = cell->Index;
  raw.TileSpanM = 1000.0;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  const auto run = [&](const RawTile &input, CountingMesh &mesher) {
    auto store = std::make_shared<Data::ArtifactStore>(
        Data::ArtifactStore::Config{.Directory = root.string()});
    Tasks compute(1), io(1);
    StructureBuildTask task(
        0,
        std::make_unique<RawTile>(input),
        heights,
        std::make_unique<StructureBuildTask::Output>(),
        mesher.Scratch(),
        std::nullopt,
        StructureBuildTask::CacheRequest{.Store = store,
                                         .Io = &io,
                                         .Source = std::nullopt,
                                         .SourceKey = 1,
                                         .ResidentBytesMost = kStructureArtifactBytesMost});
    task.Start(compute, mesher);
    for (size_t slice = 0; slice < 128; ++slice) {
      task.Join(compute);
      if (!task.Result().Status || task.Result().Tile) { break; }
      task.Resume(compute, mesher);
    }
    task.RequestStop();
    task.Join(compute);
    return task.TakeOutput();
  };
  CountingMesh first;
  auto cold = run(raw, first);
  CHECK(cold->Status && cold->Tile && !cold->CacheHit && first.Calls > 0,
        "cold request bakes and publishes its complete product");
  CountingMesh second;
  raw.Eye = {.LongitudeDeg = 10, .LatitudeDeg = 48};
  raw.FocalPx = 1234;
  auto warm = run(raw, second);
  CHECK(warm->Status && warm->Tile && warm->CacheHit && second.Calls == 0,
        "fresh store, tasks and producer load explicit LOD without invoking mesher");
  if (cold->Tile && warm->Tile) {
    CHECK(cold->Tile->Digest == warm->Tile->Digest && cold->Tile->Prints == warm->Tile->Prints,
          "persistent reload preserves geometry and semantics");
  }
  const auto key = StructureArtifactKey(raw, *heights, std::nullopt, second.ArtifactVersion());
  CHECK(key.has_value(), "complete manifest has an identity");
  CHECK(StructureArtifactKey(
            raw, *Ground::HeightField::Of(1, {block}), std::nullopt, second.ArtifactVersion()) !=
            key,
        "sampling zoom participates even when supplied block bytes match");
  auto changedBlock = block;
  changedBlock.Nodes.front() = 1.0f;
  CHECK(StructureArtifactKey(raw,
                             *Ground::HeightField::Of(0, {changedBlock}),
                             std::nullopt,
                             second.ArtifactVersion()) != key,
        "DEM sample bytes participate in identity");
  CHECK(StructureArtifactKey(raw, *heights, std::nullopt, "changed-producer") != key,
        "producer version invalidates derived content");
  CHECK(!StructureArtifactKey(raw, *heights, std::nullopt, ""),
        "unversioned producer cannot create a cache identity");
  if (key) { std::ofstream(root / (*key + ".asset"), std::ios::binary) << "broken"; }
  CountingMesh repair;
  auto repaired = run(raw, repair);
  CHECK(repaired->Status && repaired->Tile && !repaired->CacheHit && repair.Calls > 0,
        "corrupt artifact is rebuilt instead of published");
  raw.Structures.front().HeightM += 1;
  CountingMesh changed;
  auto rebuilt = run(raw, changed);
  CHECK(rebuilt->Status && rebuilt->Tile && !rebuilt->CacheHit && changed.Calls > 0,
        "changed source geometry invalidates the cached product");
  std::filesystem::remove_all(root);
  return Report();
}
