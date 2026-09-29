#include "Check.h"
#include "StructureBuildTask.h"
#include "BuildingMesh.h"
#include <array>
#include <cmath>
#include <memory>
#include <utility>

namespace {
using namespace outshine;

struct Scratch final : MeshScratch {};

class Planes final : public StructureMesher {
public:
  explicit Planes(bool refuse = false) : Refuse_(refuse) {}

  std::unique_ptr<MeshScratch> Scratch() const override { return std::make_unique<::Scratch>(); }

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &, Raised &into) const noexcept override {
    if (Refuse_) { return std::unexpected(StructureMeshError::UnsupportedFootprint); }
    const float z = plan.Coarseness == LevelOfDetail::Fine ? 0.0f : 2.0f;
    const auto first = static_cast<uint32_t>(into.RoofCorners.size());
    for (const Vec3f p : std::array<Vec3f, 3>{{{{0, 0, z}}, {{1, 0, z}}, {{0, 1, z}}}}) {
      into.RoofCorners.push_back(StoredVertex::Of(p, {}, {{0, 0, 1}}));
    }
    into.RoofRun.insert(into.RoofRun.end(), {first, first + 1, first + 2});
    return {};
  }

private:
  bool Refuse_;
};

std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  return Ground::HeightField::Of(0, {std::move(block)});
}

std::unique_ptr<Generators::RawTile> Raw(LevelOfDetail detail) {
  auto raw = std::make_unique<Generators::RawTile>();
  raw->LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  const auto cell = Generators::StructureCellOf(
      {.MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008}, raw->LatLon);
  raw->Structures.push_back({.PointCount = 4, .Cell = *cell, .HeightM = 12});
  raw->RequestedCell = cell->Index;
  raw->RequestedDetail = detail;
  raw->TileSpanM = 1000;
  return raw;
}

StructureBuildTask Task(const StructureMesher &mesher,
                        LevelOfDetail detail = LevelOfDetail::Shell,
                        uint64_t key = 71,
                        bool invalidRing = false) {
  auto raw = Raw(detail);
  if (invalidRing) { raw->Structures[0].PointCount = 2; }
  return {3,
          std::move(raw),
          Heights(),
          std::make_unique<StructureBuildTask::Output>(),
          mesher.Scratch(),
          StructureBuildTask::ProofRequest{.SourceKey = key}};
}

void Complete(StructureBuildTask &task, Tasks &pool, const StructureMesher &mesher) {
  for (size_t post = 0; post < 4096 && task.Result().Status && !task.Result().Tile; ++post) {
    task.Resume(pool, mesher);
    task.Join(pool);
    CHECK(task.Result().LastProofWork <= 8192 && task.Result().LastRanges <= 4,
          "each post respects its work allowance across phase transitions");
  }
}
}

int main() {
  using namespace outshine::Test;
  Tasks pool(1);
  Planes mesher;
  for (const auto detail : {LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    auto task = Task(mesher, detail);
    task.Start(pool, mesher);
    task.Join(pool);
    CHECK(task.Result().Status && !task.Result().Tile && task.Raw().RequestedDetail == detail,
          "finished variant stays private and its immutable request does not become Fine");
    task.Resume(pool, mesher);
    StructureBuildTask moved(std::move(task));
    moved.Join(pool);
    CHECK(!moved.Result().Tile && moved.Result().Status && !task.Running(),
          "running reference ownership moves without publishing a variant or cancelling work");
    moved.Resume(pool, mesher);
    StructureBuildTask proving(std::move(moved));
    proving.Join(pool);
    Complete(proving, pool, mesher);
    const auto &tile = proving.Result().Tile;
    CHECK(proving.Result().Status && tile && tile->SurfaceError && !tile->SurfaceFailure,
          "paired complete planes publish a certified variant");
    if (tile && tile->SurfaceError) {
      CHECK(
          tile->SurfaceError->LowerDistanceM() <= 2 && tile->SurfaceError->UpperDistanceM() >= 2 &&
              tile->SurfaceError->UpperDistanceM() - tile->SurfaceError->LowerDistanceM() <= 0.02 &&
              tile->SurfaceError->Upper.SourceKey == 71 && tile->RequestedDetail == detail &&
              tile->RequestedCell == proving.Raw().RequestedCell,
          "independent two-metre separation survives phase ownership and product transfer");
    }
  }
  for (size_t phase = 0; phase < 2; ++phase) {
    auto task = Task(mesher);
    task.Start(pool, mesher);
    task.Join(pool);
    if (phase == 1) {
      task.Resume(pool, mesher);
      task.Join(pool);
    }
    task.RequestStop();
    task.Resume(pool, mesher);
    task.Join(pool);
    CHECK(!task.Result().Status && !task.Result().Tile,
          "cancellation before reference or proof cannot expose a private variant");
  }
  Planes unsupported(true);
  for (const bool invalidRing : {false, true}) {
    const StructureMesher &chosen = invalidRing ? mesher : unsupported;
    auto task = Task(chosen, LevelOfDetail::Shell, 71, invalidRing);
    task.Start(pool, chosen);
    task.Join(pool);
    CHECK(task.Result().Tile && !task.Result().Tile->SurfaceError &&
              task.Result().Tile->SurfaceFailure ==
                  Generators::StructureSurfaceErrorFailure::InvalidGeometry,
          "identical missing surfaces from refused meshes or skipped rings never certify zero");
  }
  auto unknown = Task(mesher, LevelOfDetail::Shell, 0);
  unknown.Start(pool, mesher);
  unknown.Join(pool);
  CHECK(unknown.Result().Tile && !unknown.Result().Tile->SurfaceError &&
            unknown.Result().Tile->SurfaceFailure ==
                Generators::StructureSurfaceErrorFailure::InvalidSource,
        "missing source identity retains a variant without claiming a certificate");
  Generators::BuildingMesh native;
  auto task = Task(native);
  task.Start(pool, native);
  task.Join(pool);
  Complete(task, pool, native);
  CHECK(task.Result().Status && task.Result().Tile && task.Result().Tile->SurfaceError &&
            task.Result().Tile->SurfaceError->UpperDistanceM() >= 0,
        "native cell generation traverses the paired worker path to a finite certificate");
  return Report();
}
