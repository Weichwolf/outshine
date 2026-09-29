#include "Check.h"
#include "StructureSourcePreparation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <semaphore>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using State = StructureSourcePreparation::State;
  Tasks pool(1);
  auto field = std::make_shared<Ground::TerrainField>(3, 3);
  field->AddSource({.Kind = Data::DataKind::Elevation,
                    .Tile = {.Zoom = 4, .X = 0, .Y = 0},
                    .SourceId = "source",
                    .Revision = "original"});
  for (uint32_t row = 0; row < 3; ++row) {
    for (uint32_t column = 0; column < 3; ++column) {
      field->SetM(row, column, static_cast<float>(row * 10u + column));
    }
  }
  Ground::HeightField::Request request{.Zoom = 4};
  std::vector<SourcedTerrainFields::Entry> entries;
  std::vector<Ground::HeightField::Block> expectedBlocks;
  for (uint32_t x = 0; x < 9; ++x) {
    const Data::TileId tile{.Zoom = 4, .X = x, .Y = 0};
    entries.emplace_back(tile, field);
    request.Tiles.push_back({.Zoom = 4, .X = x, .Y = 0});
    Ground::HeightField::Block block;
    CHECK(Ground::HeightField::CopiesField(*field, tile, block), "independent copied reference");
    expectedBlocks.push_back(std::move(block));
  }
  const auto expected = Ground::HeightField::Of(4, std::move(expectedBlocks));
  const SourcedTerrainFields sources(entries);
  constexpr size_t budget = 1024u * 1024u;
  CHECK(sources.FitsPreparation(request.Tiles, budget), "source and assembly fit the reservation");
  StructureSourcePreparation refused(pool, request, sources, sources.RetainedBytes());
  CHECK(refused.Advance() == State::OverBudget && !refused.Running() && !refused.Result(),
        "source bytes alone do not admit temporary metadata or result construction");
  StructureSourcePreparation preparation(pool, request, sources, budget);
  std::binary_semaphore release(0);
  const auto blocker = pool.Post([&release] { release.acquire(); });
  CHECK(preparation.Advance() == State::Preparing && preparation.Running() && !preparation.Result(),
        "posting does not publish a field before the worker runs");
  StructureSourcePreparation moved(std::move(preparation));
  CHECK(!preparation.Running() && preparation.Advance() == State::Cancelled,
        "running move revokes the old task owner");
  release.release();
  pool.Wait(blocker);
  State state = State::Preparing;
  for (size_t attempt = 0; attempt < 100 && state == State::Preparing; ++attempt) {
    (void)pool.AwaitCompletion(0.02);
    state = moved.Advance();
  }
  const auto result = moved.Result();
  CHECK(state == State::Ready && result && result->Blocks().size() == 9 &&
            result->RasterDigest() == expected->RasterDigest() &&
            std::ranges::equal(result->Sources(), expected->Sources()),
        "all slices preserve request order, source identity and every raster bit");
  if (result) {
    CHECK(std::ranges::all_of(result->Blocks(),
                              [&field](const auto &block) {
                                return block.Terrain == field && block.Nodes.empty();
                              }),
          "exact immutable rasters are shared rather than cloned for every cell");
  }
  StructureSourcePreparation cancelled(pool, request, sources, budget);
  const auto cancellationBlocker = pool.Post([&release] { release.acquire(); });
  CHECK(cancelled.Advance() == State::Preparing, "cancellable preparation is posted");
  StructureSourcePreparation cancellationOwner(std::move(cancelled));
  cancellationOwner.Cancel();
  release.release();
  pool.Wait(cancellationBlocker);
  state = State::Preparing;
  for (size_t attempt = 0; attempt < 100 && state == State::Preparing; ++attempt) {
    (void)pool.AwaitCompletion(0.02);
    state = cancellationOwner.Advance();
  }
  CHECK(state == State::Cancelled && !cancellationOwner.Result(),
        "cancelling the moved owner prevents publication");
  return Report();
}
