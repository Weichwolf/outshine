#include "Check.h"
#include "PrismPreparationFixture.h"
#include "UrbanPrismSlot.h"
#include "Sha256.h"
#include <chrono>
#include <expected>
#include <future>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Fixture = PrismPreparationFixture;

UrbanPrismPreparation::Capture Capture(const std::string &request, size_t &calls) {
  return [request, &calls](std::stop_token) -> std::optional<UrbanPrismPreparation::Captured> {
    ++calls;
    return AssetFactory{[request](size_t most) {
      return Generators::Osm::PackageUrbanPrisms(Fixture::Product(), request, {.BytesMost = most});
    }};
  };
}
}

int main() {
  Fixture::Cache cache;
  Tasks pool(1);
  const auto first = Sha256Hex("prism slot/original");
  const auto canceled = Sha256Hex("prism slot/obsolete terrain");
  const auto latest = Sha256Hex("prism slot/latest terrain");
  UrbanPrismSlot slot(pool, cache.Database());
  size_t captures = 0;
  CHECK(Fixture::Await(slot, first, Capture(first, captures)), "initial demand becomes resident");
  const auto original = slot.Ready();
  CHECK(original && slot.Costs().Started == 1 && captures == 1,
        "one initial lookup and capture produce one resident product");

  std::promise<void> release;
  auto gate = release.get_future().share();
  CHECK(pool.PostDetached([gate] { (void)gate.wait_for(std::chrono::seconds(3)); }),
        "a bounded controlled worker holds the next lookup in its queue");
  CHECK(slot.Advance(canceled), "a replacement lookup is admitted without blocking");
  bool preserved = true;
  for (size_t index = 0; index < 1000; ++index) {
    const auto request = Sha256Hex("prism slot/intermediate/" + std::to_string(index));
    const auto update = slot.Advance(request, Capture(request, captures));
    preserved = preserved && update && !*update && slot.Ready() == original;
  }
  CHECK(preserved && slot.Costs().Started == 2 && slot.Costs().Canceled == 1 && captures == 1,
        "one thousand changes retain ready data and queue no job behind retiring work");
  CHECK(slot.Advance(latest, Capture(latest, captures)), "latest demand waits without capturing");
  release.set_value();
  CHECK(Fixture::Await(slot, latest, Capture(latest, captures)),
        "only the latest replacement becomes ready");
  CHECK(slot.Costs().Started == 3 && slot.Costs().Replaced == 2 && captures == 2 &&
            slot.ReadyKey() == latest,
        "replacement starts after retirement, without generating intermediate terrain versions");
  auto stored = AssetCache::Open(cache.Database());
  CHECK(stored, "the native cache remains readable after canceled lookup retirement");
  if (stored) {
    const auto absent = (*stored)->FindRequest(canceled);
    CHECK(absent && !*absent, "the obsolete demand publishes no product under its old key");
  }
  CHECK(!slot.Advance("unbound") && slot.ReadyKey() == latest,
        "invalid demand cannot evict the last ready representation");
  slot.Clear();
  CHECK(!slot.Ready() && slot.ReadyKey().empty(), "explicit release removes the resident product");
  CHECK(Fixture::Await(slot, first),
        "a return to a cached region requires no capture or generation");
  return Report();
}
