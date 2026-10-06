#include "Check.h"
#include "TerrainDeformationTask.h"
#include "PreparedTerrainAssets.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-terrain-cancellation-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(path);
  Data::ContentStore store({.Directory = (path / "sources").string()});
  Data::SourceSet sources(store);
  auto cache = PreparedTerrainAssets::Open((path / "assets").string(), sources);
  CHECK(cache.has_value(), "native cache opens for cancellation fixture");
  if (!cache) { return Report(); }
  constexpr TerrainPageLayout layout{.Side = 3, .Halo = 1};
  Patchwork input;
  input.Sheets.push_back({.Tile = {.Zoom = 20, .X = 1u << 19u, .Y = 1u << 19u},
                          .Nodes = std::vector<float>(layout.NodeCount(), 0),
                          .Side = layout.Side,
                          .Postings = 3});
  const auto identity = TerrainDeformationKey(input, {}, TangentFrame::At({}), layout, 30);
  CHECK(identity.has_value(), "complete terrain input identity validates");
  if (!identity) { return Report(); }
  const auto &key = *identity;
  Tasks pool(1);
  std::binary_semaphore entered(0), release(0), closed(0);
  const auto blocker = pool.Post([&] {
    entered.release();
    release.acquire();
  });
  entered.acquire();
  auto task = std::make_unique<TerrainDeformationTask>(
      pool, *cache, std::vector<EarthworkStamp>{}, input, TangentFrame::At({}), layout, 30);
  CHECK(input.Sheets.empty(), "worker owns the complete page input without a second copy");
  std::thread closer([&] {
    task.reset();
    closed.release();
  });
  CHECK(closed.try_acquire_for(std::chrono::milliseconds(100)),
        "cancellation returns while the worker is still blocked on independent work");
  CHECK((*cache).use_count() > 1,
        "queued callback retains its complete native inputs and cache owner after cancellation");
  release.release();
  closer.join();
  pool.Wait(blocker);
  const auto drained = pool.Post([] {});
  pool.Wait(drained);
  CHECK((*cache)->Costs().DeformationWrites == 0 && (*cache)->Costs().DeformationMisses == 0,
        "cancelled queued work neither calls the asset factory nor publishes a partial product");
  const auto absent = (*cache)->LoadDeformation(key);
  CHECK(absent && !*absent, "cancelled native product stays absent");
  cache->reset();
  std::filesystem::remove_all(path);
  return Report();
}
