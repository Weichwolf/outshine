#ifndef OUTSHINE_TEST_URBANPRISMSLOT_FIXTURE_H
#define OUTSHINE_TEST_URBANPRISMSLOT_FIXTURE_H
#include "UrbanPrismSlot.h"
#include "PreparedUrbanAreas.h"
#include "PreparedOsmTiles.h"
#include "OsmCacheFixture.h"
#include "test/outshine/src/generators/osm/PreparedUrbanAreas/UrbanAssetFixture.h"
#include "test/outshine/src/generators/osm/UrbanPrismPreparation/PrismPreparationFixture.h"
#include "SourceSession.h"
#include "SourceSet.h"
#include <atomic>
#include <memory>
#include <utility>
#include <chrono>
#include <expected>
#include <string>
#include <thread>

namespace outshine::Test::PrismPreparationFixture {
struct Sources {
  std::atomic_uint Calls{0};
  UrbanAssetFixture::NoTransport Wire;
  std::shared_ptr<Data::SourceSession> Session;

  std::shared_ptr<Generators::Osm::PreparedUrbanAreas> Open(Tasks &pool, const Cache &cache) {
    auto session = OsmCache::OpenSession(std::make_unique<UrbanAssetFixture::VectorSource>(
        Calls, UrbanAssetFixture::Availability::Absent));
    CHECK(session, "cache-only prism owner has an offline source session");
    if (!session) { return {}; }
    Session = std::move(*session);
    auto tiles =
        Generators::Osm::PreparedOsmTiles::Open(cache.Directory().string(), Session, Wire, pool);
    CHECK(tiles, "offline source cache opens for the actual urban generator");
    if (!tiles) { return {}; }
    auto urban = Generators::Osm::PreparedUrbanAreas::Open(
        cache.Directory().string(), Session->Sources(), pool, std::move(*tiles));
    CHECK(urban, "the actual urban generator owns its prism store");
    return urban ? std::move(*urban) : nullptr;
  }
};

inline std::expected<void, std::string>
Await(UrbanPrismSlot &slot,
      const std::string &request,
      const Generators::Osm::UrbanPrismPreparation::Capture &capture = {}) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < until) {
    const auto result = slot.Advance(request, capture);
    if (!result) { return std::unexpected(result.error()); }
    if (slot.ReadyKey() == request) { return {}; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return std::unexpected("prism slot did not deliver the requested product");
}

}
#endif
