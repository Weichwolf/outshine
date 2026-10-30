#include "TerrainFieldPreparation.h"
#include "Check.h"
#include "PreparedTerrainAssets.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Tasks.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

Ground::TerrainGrid Product(Data::TileId at) {
  Ground::TerrainField field(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t col = 0; col < 2; ++col) { field.SetM(row, col, 100); }
  }
  field.AddSource({.Tile = at, .SourceId = "concurrent-terrain", .Revision = "1"});
  return Ground::TerrainGrid::Holding(std::move(field));
}

void Exercise(const std::shared_ptr<Generators::PreparedTerrainAssets> &assets) {
  constexpr unsigned kWorkers = 6;
  Tasks workers(kWorkers);
  std::mutex mutex;
  std::condition_variable changed;
  unsigned entered = 0;
  bool released = false;
  std::array<std::optional<Ground::TerrainGrid>, kWorkers> products;
  std::array<Tasks::Handle, kWorkers> jobs{};
  for (unsigned at = 0; at < kWorkers; ++at) {
    jobs[at] = workers.Post([&, at] {
      const Data::TileId tile{.Zoom = 4, .X = at, .Y = 0};
      products[at] = assets->RequestField(tile, {})
                         ->Resume([&] {
                           {
                             std::unique_lock lock(mutex);
                             ++entered;
                             changed.notify_all();
                             changed.wait(lock, [&] { return released; });
                           }
                           return Product(tile);
                         })
                         .Grid;
    });
  }
  {
    std::unique_lock lock(mutex);
    CHECK(changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered == kWorkers; }),
          "independent terrain factories enter together without a cache-wide generation lock");
    released = true;
  }
  changed.notify_all();
  for (const auto job : jobs) { workers.Wait(job); }
  for (const auto &product : products) {
    CHECK(product && product->TryField() && product->TryField()->AtM(0, 0) == 100,
          "every independent terrain product retains its complete physical samples");
  }
  std::atomic<unsigned> generations{0};
  for (unsigned at = 0; at < kWorkers; ++at) {
    jobs[at] = workers.Post([&, at] {
      const Data::TileId tile{.Zoom = 4, .X = 8, .Y = 0};
      const auto request = assets->RequestField(tile, {});
      const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      do {
        products[at] = request
                           ->Resume([&] {
                             ++generations;
                             return Product(tile);
                           })
                           .Grid;
        if (products[at]->Where() != Ground::TerrainGrid::State::Deferred) { break; }
        std::this_thread::yield();
      } while (std::chrono::steady_clock::now() < until);
    });
  }
  for (const auto job : jobs) { workers.Wait(job); }
  CHECK(generations == 1,
        "equal simultaneous terrain requests generate their immutable product once");
  const auto &first = products[0];
  CHECK(first.has_value(), "the shared terrain request produces a value");
  if (!first) { return; }
  const auto *shared = first->TryField();
  for (const auto &product : products) {
    CHECK(product && product->TryField() && product->TryField() == shared,
          "equal consumers share the same retained terrain product");
  }
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-concurrent-terrain-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    outshine::Data::RawSourceCache::Config config{};
    config.Directory = (root / "sources").string();
    outshine::Data::RawSourceCache store(config);
    const outshine::Data::SourceSet sources(store);
    auto assets =
        outshine::Generators::PreparedTerrainAssets::Open((root / "assets").string(), sources);
    CHECK(assets, "the independent terrain cache opens");
    if (assets) { Exercise(*assets); }
  }
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
