#include "GroundPatchPreparation.h"
#include "Check.h"
#include "SourceSet.h"
#include <chrono>
#include <filesystem>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Generators;

class Heights final : public GroundQuery {
public:
  explicit Heights(Tile region) : Region(region) {}

  GroundSample At(LongitudeLatitude at) const override {
    ++Samples;
    return Waiting ? GroundSample::Waiting() : GroundSample::At(at.LatitudeDeg + at.LongitudeDeg);
  }

  GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override {
    ++Blocks;
    return {};
  }

  double PostM(double) const override { return Region.SpanNm() / 4; }

  int BlockZoom() const override { return Region.Zoom() + 1; }

  Tile Region;
  mutable size_t Samples = 0, Blocks = 0;
  bool Waiting = false;
};

std::shared_ptr<const GroundPatch> Await(GroundPatchPreparation &job, const GroundQuery &heights) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < until) {
    Snapped how = Snapped::Waiting;
    auto result = job.Advance(heights, &how);
    CHECK(result.has_value(), "worker failures propagate instead of claiming readiness");
    if (!result) { return {}; }
    if (*result) {
      CHECK(how == Snapped::Taken, "only the complete persisted patch is ready");
      return *result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(false, "sampling patch preparation finishes within the bounded fixture deadline");
  return {};
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-ground-patch-job-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  const auto region = Tile::Of(14, {.LongitudeDeg = 16.3738, .LatitudeDeg = 48.2082});
  const Data::TileId at{.Zoom = region.Zoom(),
                        .X = static_cast<uint32_t>(region.X()),
                        .Y = static_cast<uint32_t>(region.Y())};
  auto opened = PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "native sampling cache opens");
  if (!opened) { return Test::Report(); }
  auto assets = std::move(*opened);
  const auto key = assets->PatchKey(at, {}, 5, 15);
  std::shared_ptr<const GroundPatch> reference;
  {
    Tasks pool(1);
    Heights heights(region);
    GroundPatchPreparation cold(pool, assets, key, region, 5);
    reference = Await(cold, heights);
    CHECK(reference && heights.Samples == 25 && heights.Blocks == 0 &&
              assets->Costs().PatchMisses == 1 && assets->Costs().PatchWrites == 1,
          "miss samples the proper finer grid once and stores before exposing readiness");
    Snapped how = Snapped::Waiting;
    auto repeated = cold.Advance(heights, &how);
    CHECK(repeated && *repeated == reference && heights.Samples == 25,
          "repeated ready polls share the immutable product without query or disk work");
  }
  assets.reset();
  opened = PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "restart uses a fresh cache connection");
  if (!opened) { return Test::Report(); }
  assets = std::move(*opened);
  {
    Tasks pool(1);
    Heights heights(region);
    heights.Waiting = true;
    GroundPatchPreparation warm(pool, assets, key, region, 5);
    auto loaded = Await(warm, heights);
    CHECK(loaded && reference && loaded->HeightAslM({}) == reference->HeightAslM({}) &&
              heights.Samples == 0 && heights.Blocks == 0 && assets->Costs().PatchHits == 1 &&
              assets->Costs().PatchWrites == 0,
          "a fresh offline hit bypasses all height queries and generation");
    auto changed = assets->PatchKey(at, {}, 5, 14);
    GroundPatchPreparation pending(pool, assets, changed, region, 5);
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (heights.Samples == 0 && std::chrono::steady_clock::now() < until) {
      Snapped how = Snapped::Taken;
      auto result = pending.Advance(heights, &how);
      CHECK(result && !*result && how == Snapped::Waiting,
            "pending terrain never becomes a persisted partial patch");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(heights.Samples != 0 && assets->Costs().PatchWrites == 0,
          "a real miss waits for its terrain without writing incomplete samples");
    heights.Waiting = false;
    CHECK(Await(pending, heights) && assets->Costs().PatchWrites == 1,
          "settled inputs resume the same miss and publish exactly once");
  }
  assets.reset();
  std::filesystem::remove_all(root);
  return Test::Report();
}
