#include "Check.h"
#include "PrismPreparationFixture.h"
#include "Sha256.h"
#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  namespace Fixture = PrismPreparationFixture;
  Fixture::Cache cache;
  Tasks pool(1);
  const auto request = Sha256Hex("urban-prism-job/source/terrain/selection/lod");
  const auto mainThread = std::this_thread::get_id();
  size_t captures = 0;
  std::atomic_uint32_t generations{0};
  std::atomic<bool> builtOnWorker{false};
  bool capturedOnMain = true;
  {
    UrbanPrismPreparation preparation(pool, cache.Database(), request);
    const UrbanPrismPreparation::Capture capture =
        [&](std::stop_token) -> std::optional<UrbanPrismPreparation::Captured> {
      capturedOnMain &= std::this_thread::get_id() == mainThread;
      if (++captures < 3) { return std::nullopt; }
      const auto immutableInput =
          std::make_shared<const Generators::Osm::UrbanPrisms>(Fixture::Product());
      return AssetFactory{[&, immutableInput](size_t budget) {
        ++generations;
        builtOnWorker.store(std::this_thread::get_id() != mainThread);
        return Generators::Osm::PackageUrbanPrisms(
            *immutableInput, request, {.Level = 2, .BytesMost = budget});
      }};
    };
    const auto ready = Fixture::Await(preparation, capture);
    CHECK(ready && *ready,
          "a cold async request waits for inputs and delivers a complete native asset");
    CHECK(captures == 3 && generations == 1 && capturedOnMain && builtOnWorker,
          "only the observed miss captures immutable inputs; building runs once on the worker");
    CHECK(preparation.Measurements().Generations == 1 && preparation.Measurements().ReadBytes > 0,
          "worker work and actual ready bytes remain measurable");
  }
  {
    UrbanPrismPreparation preparation(pool, cache.Database(), request);
    const auto ready = Fixture::Await(
        preparation, [&](std::stop_token) -> std::optional<UrbanPrismPreparation::Captured> {
          ++captures;
          return std::unexpected("a cache hit must never capture live terrain");
        });
    CHECK(ready && *ready && (*ready)->Geometry.Samples->front().Lower == 17,
          "a fresh async owner restores native geometry with no source or terrain");
    CHECK(captures == 3 && generations == 1 && preparation.Measurements().Generations == 0,
          "a warm hit performs neither snapshot capture nor generation");
    CHECK(preparation.Key() == request, "the owner can compare persistent demand identities");
  }
  return Report();
}
