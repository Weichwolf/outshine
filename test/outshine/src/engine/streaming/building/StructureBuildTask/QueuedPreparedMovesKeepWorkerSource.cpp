#include "StructureBuildTask.h"
#include "PreparedStructureTile.h"
#include "BuildingMesh.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <semaphore>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::shared_ptr<const Generators::PreparedStructureTile> Prepared() {
  Generators::RawTile raw;
  raw.TileSpanM = 1000;
  raw.LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  raw.Structures.push_back({.PointCount = 4,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 6,
                            .Pitched = 0,
                            .WallColour = {},
                            .Facade = {}});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {0, 0, 0, 0};
  const auto heights = Ground::HeightField::Of(0, {block});
  if (!heights) {
    CHECK(false, "constant fixture heights are available");
    return {};
  }
  auto base = Generators::PrepareStructureTile(raw, *heights);
  CHECK(base.has_value(), "the queued input is a valid prepared building product");
  if (!base) { return {}; }
  return std::make_shared<Generators::PreparedStructureTile>(std::move(*base));
}
enum class Transfer { Construct, Assign };

void Run(const std::shared_ptr<const Generators::PreparedStructureTile> &base, Transfer transfer) {
  Tasks pool(1);
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  const auto blocker = pool.Post([&] {
    entered.release();
    release.acquire();
  });
  CHECK(blocker != Tasks::kNoTask && entered.try_acquire_for(std::chrono::seconds(5)),
        "the worker is held before a prepared task starts");
  const Generators::BuildingMesh mesher;
  std::optional<StructureBuildTask> moved;
  {
    auto view = std::make_unique<Generators::RawTile>();
    view->RequestedDetail = LevelOfDetail::Shell;
    StructureBuildTask original(
        1, base, std::move(view), std::make_unique<StructureBuildTask::Output>(), mesher.Scratch());
    original.Start(pool, mesher);
    if (transfer == Transfer::Construct) {
      moved.emplace(std::move(original));
    } else {
      moved.emplace(2,
                    base,
                    std::make_unique<Generators::RawTile>(),
                    std::make_unique<StructureBuildTask::Output>(),
                    mesher.Scratch());
      *moved = std::move(original);
    }
    CHECK(moved->Running() && moved->PreparedBase() == base.get(),
          "the active owner retains the same prepared source");
  }
  release.release();
  if (moved) { moved->Join(pool); }
  pool.Wait(blocker);
  CHECK(moved && moved->Result().Status && moved->Result().Tile,
        "a queued move cannot send prepared work into the absent raw-height path");
}
}

int main() {
  const auto base = Prepared();
  if (base) {
    Run(base, Transfer::Construct);
    Run(base, Transfer::Assign);
  }
  return Report();
}
