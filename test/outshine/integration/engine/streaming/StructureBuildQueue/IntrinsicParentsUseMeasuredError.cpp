#include "test/harness/engine/NativeBuildingSources.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "PreparedBuildingAssets.h"
#include "StructureBuildTask.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr uint64_t kSource = 73;

class UnavailableMesher final : public BuildingMesh {
public:
  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &, MeshScratch &, Raised &) const noexcept override {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
};

class ShellOnlyMesher final : public BuildingMesh {
public:
  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    if (plan.Coarseness == LevelOfDetail::Fine) {
      return std::unexpected(StructureMeshError::InvalidPlan);
    }
    return BuildingMesh::Mesh(plan, scratch, into);
  }
};

std::optional<BakedTile> Produce(BuildingSources::Fixture &fixture,
                                 const std::string &key,
                                 const PreparedBuildingAssets::Base &base,
                                 Vec3 eye,
                                 double errorPx,
                                 const std::shared_ptr<const StructureMesher> &mesher) {
  auto raw = std::make_unique<RawTile>();
  raw->Eye = fixture.Eye;
  raw->EyeEcef = eye;
  raw->Projection = {.FocalPx = 720, .AllowedErrorPx = errorPx};
  raw->RequestedDetail = LevelOfDetail::Shell;
  StructureBuildTask work(JobLease(),
                          0,
                          base,
                          std::move(raw),
                          std::make_unique<StructureBuildTask::Output>(),
                          mesher->Scratch());
  work.UseNativeAssets(fixture.Stack.BuildingAssets(), key);
  const auto cell = base->Structures.front().Layout.Cell.Index;
  work.UseInitialCells(uint64_t{1} << (cell - 1u));
  work.UseIntrinsicParents(kSource, std::nullopt);
  work.Start(fixture.Worker, mesher);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  for (;;) {
    work.Join();
    CHECK(work.Result().Status, "native parent generation has no source or geometry error");
    if (!work.Result().Status) { return std::nullopt; }
    if (work.Result().Tile) { return std::move(work.Result().Tile); }
    if (std::chrono::steady_clock::now() >= deadline || !work.CanResume()) {
      CHECK(false, "bounded worker production reaches a complete native parent");
      return std::nullopt;
    }
    work.Resume(fixture.Worker);
  }
}
}

int main() {
  BuildingSources::Fixture fixture;
  CHECK(fixture.Open(), "actual native vector, terrain and asset fixtures open");
  if (!fixture.Stack.BuildingAssets()) { return Report(); }
  const auto key = fixture.Generate();
  CHECK(key, "canonical native source geometry prepares");
  if (!key) { return Report(); }
  const auto loaded = fixture.Stack.BuildingAssets()->Load(*key);
  CHECK(loaded && *loaded && !(**loaded).Structures.empty(), "prepared native structures load");
  if (!loaded || !*loaded || (**loaded).Structures.empty()) { return Report(); }
  const auto &base = *loaded;
  const Vec3 far = base->AnchorEcef + Vec3{{1000000, 1000000, 1000000}};
  const auto coarse = Produce(fixture, *key, base, far, 1, std::make_shared<ShellOnlyMesher>());
  CHECK(coarse && coarse->RequestedDetail == LevelOfDetail::Shell && !coarse->SelectedView &&
            coarse->SurfaceError && !coarse->SurfaceFailure && coarse->WorldBounds &&
            coarse->SurfaceError->Upper.SourceKey == kSource && !coarse->Built.WallRun.empty() &&
            !coarse->Built.RoofRun.empty(),
        "a far parent retains roofs and facades with a source-bound error without meshing Fine");
  if (!coarse || !coarse->SurfaceError) { return Report(); }
  const auto cached = Produce(fixture, *key, base, far, 1, std::make_shared<UnavailableMesher>());
  CHECK(cached && cached->SurfaceError && cached->WorldBounds &&
            cached->RequestedDetail == LevelOfDetail::Shell &&
            cached->SurfaceError->Upper.SourceKey == kSource &&
            std::isfinite(cached->SurfaceError->UpperDistanceM()) &&
            cached->Prints.size() == coarse->Prints.size(),
        "a cache hit restores geometry error, bounds and every source footprint");
  const auto fine = Produce(fixture, *key, base, far, 0, std::make_shared<BuildingMesh>());
  CHECK(fine && fine->RequestedDetail == LevelOfDetail::Fine && !fine->SelectedView &&
            !fine->Built.WallRun.empty() && !fine->Built.RoofRun.empty() &&
            fine->Prints.size() == coarse->Prints.size(),
        "zero approximation allowance selects complete fine geometry from the same sources");
  return Report();
}
