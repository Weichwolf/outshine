#include "Check.h"
#include "TerrainDeformationTask.h"
#include "PreparedTerrainAssets.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr TerrainPageLayout kLayout{.Side = 3, .Halo = 1};

Patchwork Inputs() {
  Patchwork pages;
  for (uint32_t column = 0; column < 4; ++column) {
    pages.Sheets.push_back({.Tile = {.Zoom = 20, .X = (1u << 19u) + column, .Y = 1u << 19u},
                            .Nodes = std::vector<float>(kLayout.NodeCount(), 1.0f),
                            .Side = kLayout.Side,
                            .Postings = 3,
                            .Virtual = true,
                            .SourceZoom = 17});
  }
  pages.Tiles = 4;
  pages.ReachTiles = 4;
  return pages;
}

std::vector<EarthworkStamp> Stamps() {
  EarthworkStamp stamp{.RingEastNorthM = {-100, -100, 200, -100, 200, 100, -100, 100},
                       .LowE = -100,
                       .HighE = 200,
                       .LowN = -100,
                       .HighN = 100,
                       .PlateauM = 7,
                       .ApronM = 3,
                       .Fills = true,
                       .Kind = EarthworkKind::Pad};
  stamp.HoleRingsEastNorthM.push_back({0, 0, 5, 0, 5, 5, 0, 5});
  return {stamp};
}

std::expected<PressedTerrain, std::string> Complete(TerrainDeformationTask &task,
                                                    Patchwork &pages) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!task.Advance()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return std::unexpected("terrain deformation did not complete");
    }
    (void)task.AwaitSlice(0.01);
  }
  return task.Take(pages);
}
}

int main() {
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-terrain-deformation-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(path);
  Data::ContentStore store({.Directory = (path / "sources").string()});
  Data::SourceSet sources(store);
  const auto frame = TangentFrame::At({});
  Tasks pool(1);
  Patchwork reference = Inputs();
  TerrainPressJob original(Stamps(), reference, frame, kLayout, 30);
  while (!original.Advance(32, 8192)) {}
  const auto expected = original.Take();
  CHECK(expected.Nodes > 0, "fixture creates actual deformed terrain");
  const auto identity = TerrainDeformationKey(Inputs(), Stamps(), frame, kLayout, 30);
  CHECK(identity.has_value(), "complete terrain input identity validates");
  if (!identity) { return Report(); }
  const auto &key = *identity;
  CHECK(key.size() == 64, "complete input plan has a versioned content identity");

  auto cache = PreparedTerrainAssets::Open((path / "assets").string(), sources);
  CHECK(cache.has_value(), "native terrain service opens");
  if (!cache) { return Report(); }
  Patchwork cold = Inputs();
  {
    TerrainDeformationTask task(pool, *cache, Stamps(), cold, frame, kLayout, 30);
    const auto result = Complete(task, cold);
    CHECK(result && result->Nodes == expected.Nodes && result->Pads.Nodes == expected.Pads.Nodes,
          "cold asset retains deformation results and contact diagnostics");
    CHECK(cold.Sheets == reference.Sheets && cold.Tiles == 4 && cold.ReachTiles == 4,
          "native deformation reproduces the original generator bit for bit");
  }
  CHECK((*cache)->Costs().DeformationMisses == 1 && (*cache)->Costs().DeformationWrites == 1,
        "cold miss builds and atomically publishes exactly once");
  cache->reset();
  cache = PreparedTerrainAssets::Open((path / "assets").string(), sources);
  CHECK(cache.has_value(), "independent native cache connection restarts");
  if (!cache) { return Report(); }
  Patchwork warm = Inputs();
  {
    TerrainDeformationTask task(pool, *cache, Stamps(), warm, frame, kLayout, 30);
    const auto result = Complete(task, warm);
    CHECK(result && result->Nodes == expected.Nodes && result->GatherMs == 0 &&
              result->ApplyMs == 0,
          "warm native pages retain effects without running deformation stages");
    CHECK(warm.Sheets == reference.Sheets, "offline replay preserves every tile and height sample");
  }
  CHECK((*cache)->Costs().DeformationHits == 1 && (*cache)->Costs().DeformationMisses == 0 &&
            (*cache)->Costs().DeformationWrites == 0,
        "warm process loads the ready asset without a factory invocation");

  Patchwork changed = Inputs();
  changed.Sheets.front().Nodes.front() += 1;
  CHECK(TerrainDeformationKey(changed, Stamps(), frame, kLayout, 30) != key,
        "height changes invalidate just the dependent native deformation");
  auto stamps = Stamps();
  stamps.front().HoleRingsEastNorthM.front().front() += 1;
  CHECK(TerrainDeformationKey(Inputs(), stamps, frame, kLayout, 30) != key,
        "contact holes participate in the full asset identity");
  stamps = Stamps();
  stamps.front().CorridorKey = 7;
  CHECK(TerrainDeformationKey(Inputs(), stamps, frame, kLayout, 30) != key,
        "corridor ownership participates in deformation identity");
  CHECK(TerrainDeformationKey(Inputs(), Stamps(), frame, kLayout, 29) != key &&
            TerrainDeformationKey(
                Inputs(), Stamps(), TangentFrame::At({.LongitudeDeg = 1}), kLayout, 30) != key,
        "construction limit and physical frame participate in identity");
  PreparedTerrainDeformation product{.Pages = reference.Sheets, .Effects = expected};
  auto bytes = EncodeTerrainDeformation(key, product);
  CHECK(bytes.has_value(), "complete product encodes within its byte budget");
  if (bytes) {
    CHECK(!DecodeTerrainDeformation(std::string(64, 'f'), *bytes),
          "a product cannot be replayed under another input identity");
    bytes->pop_back();
    CHECK(!DecodeTerrainDeformation(key, *bytes), "truncated native pages are never ready");
  }
  product.Pages.front().Nodes.front() = std::numeric_limits<float>::infinity();
  CHECK(!EncodeTerrainDeformation(key, product), "nonfinite height pages cannot be published");
  cache->reset();
  std::filesystem::remove_all(path);
  return Report();
}
