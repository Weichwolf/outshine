#include <expected>
#include <cstddef>
#include "Check.h"
#include "PrismSlotFixture.h"
#include "UrbanPrismSlot.h"
#include "Sha256.h"
#include <chrono>
#include <future>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using outshine::Generators::Osm::UrbanPrismPreparation;
  namespace Fixture = PrismPreparationFixture;
  const Fixture::Cache cache;
  Tasks pool(1);
  UrbanPrismSlot slot(pool, cache.Database());
  const auto request = Sha256Hex("prism slot/canceled running factory");
  std::promise<void> release;
  std::promise<void> entered;
  auto gate = release.get_future().share();
  const auto running = entered.get_future();
  size_t captures = 0;
  const UrbanPrismPreparation::Capture slow =
      [&](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
    ++captures;
    return AssetFactory{[&, gate](size_t most) {
      entered.set_value();
      (void)gate.wait_for(std::chrono::seconds(3));
      return Generators::Osm::PackageUrbanPrisms(Fixture::Product(), request, {.BytesMost = most});
    }};
  };
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  bool valid = true;
  while (std::chrono::steady_clock::now() < until &&
         running.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    valid = valid && slot.Advance(request, slow).has_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(valid && running.wait_for(std::chrono::seconds(0)) == std::future_status::ready,
        "the controlled generation is actually executing before cancellation");
  slot.Clear();
  for (size_t index = 0; index < 1000; ++index) {
    const auto result = slot.Advance(request, slow);
    valid = valid && result && !*result;
  }
  CHECK(valid && slot.Costs().Started == 1 && slot.Costs().Canceled == 1 && slot.InFlight() &&
            !slot.Ready(),
        "clear and immediate return cannot start a second job while canceled generation runs");
  release.set_value();
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask && pool.AwaitCompletion(barrier, 2),
        "the old worker actually finishes before its native cache is inspected");
  CHECK(!slot.InFlight(), "canceled work stops occupying a worker slot only after completion");
  auto stored = AssetCache::Open(cache.Database());
  CHECK(stored, "the native cache remains readable after canceled generation");
  if (stored) {
    const auto absent = (*stored)->FindRequest(request);
    CHECK(absent && !*absent, "the completed canceled factory publishes no stale product");
  }
  const auto complete = Fixture::Await(
      slot,
      request,
      [&](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
        ++captures;
        return AssetFactory{[request](size_t most) {
          return Generators::Osm::PackageUrbanPrisms(
              Fixture::Product(), request, {.BytesMost = most});
        }};
      });
  CHECK(complete && captures == 2 && slot.Costs().Started == 2 && slot.Costs().Replaced == 1,
        "only a new capture completes the same demand after the old worker retires");
  const auto original = slot.Ready();
  const auto failed = Sha256Hex("prism slot/missing terrain");
  const auto failure = Fixture::Await(
      slot, failed, [](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
        return std::unexpected("terrain input unavailable");
      });
  CHECK(!failure && failure.error() == "terrain input unavailable" && slot.Ready() == original,
        "a failed replacement reports its error while retaining the previous ready product");
  for (size_t index = 0; index < 1000; ++index) {
    CHECK(!slot.Advance(failed),
          "unchanged failed demand remains explicit without automatic retries");
  }
  CHECK(slot.Costs().Started == 3, "failed unchanged demand cannot flood the worker queue");
  return Report();
}
