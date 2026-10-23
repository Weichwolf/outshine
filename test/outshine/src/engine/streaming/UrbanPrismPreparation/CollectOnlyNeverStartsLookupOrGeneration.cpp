#include "Check.h"
#include "PrismPreparationFixture.h"
#include "Sha256.h"
#include <chrono>
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
  const auto first = Sha256Hex("prism slot/credit original");
  const auto next = Sha256Hex("prism slot/credit replacement");
  size_t captures = 0;
  auto capture = [&](const std::string &request) -> UrbanPrismPreparation::Capture {
    return [&, request](std::stop_token) -> std::optional<UrbanPrismPreparation::Captured> {
      ++captures;
      return AssetFactory{[request](size_t most) {
        return Generators::Osm::PackageUrbanPrisms(
            Fixture::Product(), request, {.BytesMost = most});
      }};
    };
  };
  CHECK(slot.Advance(first, capture(first), UrbanPrismSlot::Admission::CollectOnly) &&
            slot.Costs().Started == 0 && captures == 0 && !slot.InFlight(),
        "a slot without credit neither reads the cache nor captures generation input");
  CHECK(Fixture::Await(slot, first, capture(first)), "the admitted original becomes resident");
  const auto original = slot.Ready();
  CHECK(slot.Advance(next, capture(next), UrbanPrismSlot::Admission::CollectOnly) &&
            slot.Costs().Started == 1 && slot.Ready() == original && captures == 1,
        "retargeting without credit retains ready data and does not start a replacement");
  CHECK(slot.Advance(next), "one admitted lookup may wait for inputs without generating");
  const auto fence = pool.Post([] {});
  CHECK(fence != Tasks::kNoTask && pool.AwaitCompletion(fence, 2), "lookup really finishes");
  const auto collected = slot.Advance(next, capture(next), UrbanPrismSlot::Admission::CollectOnly);
  CHECK(collected && !*collected && captures == 1 && !slot.InFlight() && slot.Ready() == original,
        "collecting a completed miss cannot invoke a capture or enqueue generation");
  CHECK(Fixture::Await(slot, next, capture(next)) && captures == 2,
        "new credit resumes the same pending lookup and completes only its latest demand");
  const auto replacement = slot.Ready();
  CHECK(slot.Advance(first), "a return to the original asset starts its cached lookup");
  const auto finalFence = pool.Post([] {});
  CHECK(finalFence != Tasks::kNoTask && pool.AwaitCompletion(finalFence, 2),
        "the warm read finishes before collect-only publication");
  const auto hit = slot.Advance(first, capture(first), UrbanPrismSlot::Admission::CollectOnly);
  CHECK(hit && *hit && captures == 2 && slot.Ready() != replacement && slot.ReadyKey() == first,
        "finished hits publish without work credit or source capture");
  return Report();
}
