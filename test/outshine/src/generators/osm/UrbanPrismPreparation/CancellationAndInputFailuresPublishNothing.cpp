#include <stop_token>
#include <expected>
#include "Check.h"
#include "PrismPreparationFixture.h"
#include "Sha256.h"
#include <chrono>
#include <filesystem>
#include <future>
#include <optional>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using outshine::Generators::Osm::UrbanPrismPreparation;
  namespace Fixture = PrismPreparationFixture;
  const Fixture::Cache cache;
  Tasks pool(1);
  const auto request = Sha256Hex("urban-prism-job/failure");
  std::promise<void> release;
  auto gate = release.get_future().share();
  CHECK(pool.PostDetached([gate] { (void)gate.wait_for(std::chrono::seconds(2)); }),
        "a bounded controlled job delays the lookup");
  {
    UrbanPrismPreparation preparation(pool, cache.Database(), request);
    preparation.Cancel();
    CHECK(!preparation.Advance(), "canceling queued work does not block for the worker");
  }
  release.set_value();
  {
    UrbanPrismPreparation preparation(pool, cache.Database(), request);
    const auto result = Fixture::Await(
        preparation, [](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
          return std::unexpected("selected terrain changed before snapshot capture");
        });
    CHECK(!result && result.error() == "selected terrain changed before snapshot capture",
          "input capture failures remain explicit and cannot become ready assets");
  }
  {
    UrbanPrismPreparation preparation(pool, cache.Database(), request);
    const auto result = Fixture::Await(
        preparation, [](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
          return AssetFactory{};
        });
    CHECK(!result && result.error() == "captured prism factory is empty",
          "an empty captured job cannot leave preparation pending forever");
  }
  auto stored = AssetCache::Open(cache.Database());
  CHECK(stored, "the worker-created cache remains readable");
  if (stored) {
    const auto record = (*stored)->FindRequest(request);
    CHECK(record && !*record, "canceled and failed requests publish no partial product");
  }
  UrbanPrismPreparation invalid(pool, cache.Database(), "missing-identity");
  CHECK(!invalid.Advance(), "invalid demand is rejected without starting IO");
  return Report();
}
