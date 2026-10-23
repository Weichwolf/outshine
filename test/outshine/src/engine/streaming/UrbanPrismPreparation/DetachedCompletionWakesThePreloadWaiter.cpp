#include "Check.h"
#include "PrismPreparationFixture.h"
#include "Sha256.h"
#include <chrono>
#include <cstddef>
#include <future>
#include <latch>
#include <optional>
#include <stop_token>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  namespace Fixture = PrismPreparationFixture;
  Fixture::Cache cache;
  Tasks pool(1);
  UrbanPrismSlot slot(pool, cache.Database());
  const auto request = Sha256Hex("detached preload completion");
  CHECK(slot.Advance(request) && slot.AwaitProgress(2), "the cache lookup wakes its own waiter");
  std::latch entered(1), release(1);
  const auto captured = slot.Advance(
      request, [&](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
        return AssetFactory{[&](size_t most) {
          entered.count_down();
          release.wait();
          return Generators::Osm::PackageUrbanPrisms(
              Fixture::Product(), request, {.BytesMost = most});
        }};
      });
  CHECK(captured && !*captured, "the selected factory runs as detached worker work");
  if (!captured || *captured) { return Report(); }
  entered.wait();
  auto waiter = std::async(std::launch::async, [&] { return slot.AwaitProgress(2); });
  CHECK(waiter.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout,
        "the preloader really waits while the worker is blocked");
  release.count_down();
  CHECK(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready && waiter.get(),
        "actual completion wakes preload before its two-second observation deadline");
  CHECK(slot.Advance(request) && slot.ReadyKey() == request && !slot.InFlight(),
        "the awakened main thread can collect the complete cached product");
  return Report();
}
