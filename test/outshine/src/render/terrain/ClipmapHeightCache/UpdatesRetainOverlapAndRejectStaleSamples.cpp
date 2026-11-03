#include "Check.h"
#include "ClipmapHeightCache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kWorkBytes = size_t{8} * 1024 * 1024;

void Fill(ClipmapHeightUpdate &update) {
  const auto points = update.Positions();
  const auto values = update.Samples();
  for (size_t i = 0; i < points.size(); ++i) {
    values[i] = static_cast<float>(10 + points[i][0] / 8 - points[i][1] / 16);
  }
}

void CheckStored(const ClipmapHeightCache &cache) {
  for (uint32_t level = 0; level < cache.Grid().Levels; ++level) {
    CHECK(cache.Complete(level), "fully known retained windows expose complete coverage");
    const auto low = cache.Origin(level);
    CHECK(low.has_value(), "committed level has an origin");
    if (!low) { continue; }
    const double step = std::ldexp(cache.Grid().CellM, static_cast<int>(level));
    for (uint32_t y = 0; y < cache.Side(); ++y) {
      for (uint32_t x = 0; x < cache.Side(); ++x) {
        const ClipmapNode node{(*low)[0] + x, (*low)[1] + y};
        const auto expected = static_cast<float>(10 + static_cast<double>(node[0]) * step / 8 -
                                                 static_cast<double>(node[1]) * step / 16);
        CHECK(cache.Height(level, node) == expected,
              "old overlap and new bands share the same world coordinates");
      }
    }
    CHECK(!cache.Height(level, {(*low)[0] - 1, (*low)[1]}),
          "old toroidal slots outside the window stay unknown");
  }
}

void Movement() {
  auto made = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 4}, kWorkBytes);
  CHECK(made.has_value(), "the caller admits bounded retained height grids");
  if (!made) { return; }
  auto &cache = *made;
  CHECK(cache.HeapBytes() <= kWorkBytes, "actual retained allocations fit admission");
  const std::array<Vec2, 7> cameras{
      {{{0, 0}}, {{0.5, 0.5}}, {{2, 2}}, {{-3, 2}}, {{4, -6}}, {{800, -1600}}, {{-2000, 3200}}}};
  const std::array<size_t, 3> expected{size_t{4} * 19 * 19, 0, size_t{4} * 19 - 4};
  for (size_t i = 0; i < cameras.size(); ++i) {
    auto update = cache.Plan(cameras[i], kWorkBytes);
    CHECK(update.has_value(), "bounded movement prepares owned source queries");
    if (!update) { continue; }
    CHECK(update->HeapBytes() <= kWorkBytes, "actual owned work allocations fit admission");
    if (i < expected.size()) {
      CHECK(update->Positions().size() == expected[i], "sampling follows only exposed bands");
    }
    Fill(*update);
    CHECK(cache.Commit(*update).has_value(), "a complete update publishes");
    if (!update->Positions().empty()) {
      CHECK(!cache.Commit(*update), "the same nonempty update cannot publish twice");
    }
    CheckStored(cache);
  }
}

void RejectionsPreserveThePreviousWindow() {
  auto made = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 2}, kWorkBytes);
  CHECK(made.has_value(), "height cache builds");
  if (!made) { return; }
  auto &cache = *made;
  auto first = cache.Plan({{0, 0}}, kWorkBytes);
  CHECK(first.has_value(), "first population prepares");
  if (!first) { return; }
  Fill(*first);
  CHECK(cache.Commit(*first).has_value(), "first population commits");
  auto pending = cache.Plan({{2, 2}}, kWorkBytes);
  auto newer = cache.Plan({{4, 4}}, kWorkBytes);
  CHECK(pending && newer, "two workers can hold independent owned updates");
  if (!pending || !newer) { return; }
  pending->Samples().front() = std::numeric_limits<float>::infinity();
  const auto bad = cache.Commit(*pending);
  CHECK(!bad && bad.error() == ClipmapHeightError::InvalidSamples,
        "infinite output is rejected before mutation");
  CHECK((cache.Origin(0) == ClipmapNode{-9, -9}), "failed output keeps the old origin");
  CheckStored(cache);
  const auto budget = cache.Plan({{800, 800}}, 1);
  CHECK(!budget && budget.error() == ClipmapHeightError::OverBudget,
        "work admission precedes query allocation");
  const size_t exact = size_t{2} * 19 * 19 * (sizeof(Vec2) + sizeof(float)) +
                       size_t{2} * sizeof(ClipmapHeightRegion);
  CHECK(cache.Plan({{800, 800}}, exact).has_value() && !cache.Plan({{800, 800}}, exact - 1),
        "derived complete-update byte count is an exact admission boundary");
  CHECK(!cache.Plan({{1e100, 0}}, kWorkBytes),
        "integer overflow is refused before converting coordinates");
  CHECK(!cache.Plan({{std::numeric_limits<double>::quiet_NaN(), 0}}, kWorkBytes),
        "invalid cameras are refused");
  Fill(*newer);
  CHECK(cache.Commit(*newer).has_value(), "the newer independent job can publish");
  Fill(*pending);
  const auto stale = cache.Commit(*pending);
  CHECK(!stale && stale.error() == ClipmapHeightError::StaleUpdate,
        "late old work cannot overwrite a newer window");
  CheckStored(cache);
  auto unknown = cache.Plan({{100, 100}}, kWorkBytes);
  CHECK(unknown.has_value(), "unknown source coverage is an explicit update");
  if (!unknown) { return; }
  CHECK(cache.Commit(*unknown).has_value(), "unknown samples remain unknown");
  CHECK(!cache.Height(0, {100, 100}), "missing source is never converted to flat ground");
  CHECK(!cache.Complete(0), "unknown samples revoke complete coverage");
}

void OwnershipSurvivesJobTransfer() {
  auto first = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 1}, kWorkBytes);
  auto second = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 1}, kWorkBytes);
  CHECK(first && second, "independent owners build the same layout");
  if (!first || !second) { return; }
  auto job = first->Plan({{0, 0}}, kWorkBytes);
  CHECK(job.has_value(), "owned update builds");
  if (!job) { return; }
  Fill(*job);
  CHECK(!second->Commit(*job), "another cache rejects a foreign job even at the same generation");
  const ClipmapHeightUpdate transferred(std::move(*job));
  CHECK(!first->Commit(*job), "a transferred-from job cannot change origins without its data");
  CHECK(first->Commit(transferred).has_value(),
        "transferring the owned job preserves its owner binding");
  auto pending = first->Plan({{2, 2}}, kWorkBytes);
  CHECK(pending.has_value(), "another update builds before moving the owner");
  if (!pending) { return; }
  Fill(*pending);
  ClipmapHeightCache relocated(std::move(*first));
  CHECK(!first->Commit(*pending) && !relocated.Commit(*pending),
        "moving a cache invalidates outstanding jobs");
  CHECK(!first->Plan({{2, 2}}, kWorkBytes), "a moved-from cache cannot admit new jobs");
  CheckStored(relocated);
}
}

int main() {
  Movement();
  RejectionsPreserveThePreviousWindow();
  OwnershipSurvivesJobTransfer();
  for (const auto grid :
       {ClipmapGrid{.HalfCells = 4, .Levels = 1},
        ClipmapGrid{.HalfCells = 8, .Levels = 0},
        ClipmapGrid{.HalfCells = 8, .Levels = 1, .CellM = 0},
        ClipmapGrid{.HalfCells = 8, .Levels = std::numeric_limits<uint32_t>::max()}}) {
    CHECK(!ClipmapHeightCache::Make(grid, kWorkBytes), "invalid layouts fail before allocating");
  }
  CHECK(!ClipmapHeightCache::Make({.HalfCells = 128, .Levels = 13}, 1024),
        "retained data also needs admission");
  return Report();
}
