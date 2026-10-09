#include "GroundPatchPreparation.h"
#include "Check.h"
#include "SourceSet.h"
#include <chrono>
#include <filesystem>
#include <semaphore>

int main() {
  using namespace outshine;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-ground-patch-cancel-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  auto opened = Generators::PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "native sampling cache opens");
  if (!opened) { return Test::Report(); }
  auto assets = std::move(*opened);
  std::binary_semaphore entered(0), resume(0);
  {
    Tasks pool(1);
    CHECK(pool.PostDetached([&] {
      entered.release();
      resume.acquire();
    }),
          "fixture holds the compute worker before the queued read");
    const bool blocked = entered.try_acquire_for(std::chrono::seconds(2));
    CHECK(blocked, "worker reaches the bounded fixture barrier");
    if (blocked) {
      const Generators::Tile region(14, 8937, 5683);
      const auto key = assets->PatchKey({.Zoom = 14, .X = 8937, .Y = 5683}, {}, 3, 13);
      GroundPatchPreparation canceled(pool, assets, key, region, 3);
    }
    resume.release();
    const auto drained = pool.Post([] {});
    CHECK(drained != Tasks::kNoTask && pool.AwaitCompletion(drained, 2),
          "destroyed reader owns its completion and never borrows the destroyed job");
    const auto costs = assets->Costs();
    CHECK(costs.PatchMisses == 0 && costs.PatchHits == 0 && costs.PatchWrites == 0,
          "queued canceled reads perform no cache IO or generation");
  }
  assets.reset();
  std::filesystem::remove_all(root);
  return Test::Report();
}
