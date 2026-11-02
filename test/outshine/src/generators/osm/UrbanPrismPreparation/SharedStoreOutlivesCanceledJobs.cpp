#include "PrismPreparationFixture.h"
#include "AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators::Osm;
namespace Fixture = PrismPreparationFixture;

void IndependentRequests() {
  const Fixture::Cache directory;
  Tasks pool(1);
  auto opened = AssetCache::Open(directory.Database());
  CHECK(opened, "one real store serves independent prism requests");
  if (!opened) { return; }
  std::shared_ptr<AssetCache> store = std::move(*opened);
  const std::weak_ptr<AssetCache> weak = store;
  const auto firstKey = Sha256Hex("shared prism store/first");
  const auto secondKey = Sha256Hex("shared prism store/second");
  auto first = std::make_unique<UrbanPrismPreparation>(pool, store, firstKey);
  auto second = std::make_unique<UrbanPrismPreparation>(pool, store, secondKey);
  store.reset();
  const auto capture = [](const std::string &key) -> UrbanPrismPreparation::Capture {
    return [key](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
      return AssetFactory{[key](size_t most) {
        return PackageUrbanPrisms(Fixture::Product(), key, {.BytesMost = most});
      }};
    };
  };
  const auto firstProduct = Fixture::Await(*first, capture(firstKey));
  CHECK(firstProduct && *firstProduct,
        "the first request completes after its caller drops the store");
  first.reset();
  CHECK(!weak.expired(), "retiring one request preserves the other request's store");
  const auto secondProduct = Fixture::Await(*second, capture(secondKey));
  CHECK(secondProduct && *secondProduct,
        "another request still publishes through the shared store");
  second.reset();
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "completed jobs can be drained");
  if (barrier != Tasks::kNoTask) { pool.Wait(barrier); }
  CHECK(weak.expired(), "the completed requests release their last store owner");
}

void QueuedCancellation() {
  const Fixture::Cache directory;
  Tasks pool(1);
  std::promise<void> release;
  const auto gate = release.get_future().share();
  CHECK(pool.PostDetached([gate] { (void)gate.wait_for(std::chrono::seconds(5)); }),
        "a bounded worker hold makes retirement observable");
  auto opened = AssetCache::Open(directory.Database());
  CHECK(opened, "the queued request starts from an existing store");
  if (!opened) {
    release.set_value();
    return;
  }
  std::shared_ptr<AssetCache> store = std::move(*opened);
  const std::weak_ptr<AssetCache> weak = store;
  auto job = std::make_unique<UrbanPrismPreparation>(
      pool, store, Sha256Hex("shared prism store/canceled queue"));
  store.reset();
  job->Cancel();
  CHECK(job->InFlight() && job->HoldsAdmission(),
        "canceled queued work keeps its original admission until the worker actually ends");
  job.reset();
  CHECK(!weak.expired(), "destroying a canceled owner cannot close a queued worker's store");
  release.set_value();
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the canceled worker can be drained");
  if (barrier != Tasks::kNoTask) { pool.Wait(barrier); }
  CHECK(weak.expired(), "the stopped worker releases the final store ownership");
}
}

int main() {
  IndependentRequests();
  QueuedCancellation();
  return outshine::Test::Report();
}
