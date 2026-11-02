#include "AssetCache.h"
#include "Check.h"
#include "JobAdmission.h"
#include "OpenAssetStore.h"
#include "OsmCacheFixture.h"
#include "PreparedBuildingAssets.h"
#include "PreparedGroundRegions.h"
#include "PreparedOsmTiles.h"
#include "PreparedRoadPages.h"
#include "PreparedStreetGraph.h"
#include "PreparedTerrainAssets.h"
#include "PreparedTransportPlans.h"
#include "PreparedUrbanAreas.h"
#include "SourceSession.h"
#include "Tasks.h"
#include "test/outshine/src/generators/osm/PreparedUrbanAreas/UrbanAssetFixture.h"
#include <atomic>
#include <chrono>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;

struct Directory {
  const std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-shared-store-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

void RejectMissingStore(const std::shared_ptr<Data::SourceSession> &session,
                        Data::SourceTransport &wire,
                        Tasks &compute) {
  const auto &sources = session->Sources();
  CHECK(!PreparedTerrainAssets::Open({}, sources) && !PreparedBuildingAssets::Open({}, sources) &&
            !PreparedGroundRegions::Open({}, sources, "rules") &&
            !PreparedStreetGraph::Open({}, sources) && !PreparedRoadPages::Open({}) &&
            !PreparedTransportPlans::Open({}) &&
            !PreparedOsmTiles::Open({}, session, wire, compute) &&
            !PreparedUrbanAreas::Open({}, sources, compute, {}),
        "every native producer requires an explicitly supplied store");
}

void SharedLifetime(const std::shared_ptr<Data::SourceSession> &session,
                    Data::SourceTransport &wire) {
  const Directory directory;
  Tasks io(1);
  Tasks compute(1);
  const auto admission = std::make_shared<JobAdmission>(1);
  auto store = OpenAssetStore(directory.Path, {.Workers = &io, .Connections = admission});
  CHECK(store, "the shared store binds without opening SQLite");
  if (!store) { return; }
  const std::weak_ptr<AssetCache> lifetime = store;
  {
    const auto &sources = session->Sources();
    const auto terrain = PreparedTerrainAssets::Open(store, sources);
    const auto buildings = PreparedBuildingAssets::Open(store, sources);
    const auto regions = PreparedGroundRegions::Open(store, sources, "rules");
    const auto network = PreparedStreetGraph::Open(store, sources);
    const auto roads = PreparedRoadPages::Open(store);
    const auto plans = PreparedTransportPlans::Open(store);
    auto vectors = PreparedOsmTiles::Open(store, session, wire, compute);
    CHECK(terrain && buildings && regions && network && roads && plans && vectors,
          "independent native product families bind the same store");
    if (!terrain || !buildings || !regions || !network || !roads || !plans || !vectors) { return; }
    auto urban = PreparedUrbanAreas::Open(store, sources, compute, *vectors);
    CHECK(urban && admission->Active() == 0 && !std::filesystem::exists(directory.Path),
          "binding all producers performs no store IO or connection admission on the caller");
    if (!urban) { return; }
    store.reset();
    (*urban)->Cancel();
    urban->reset();
    vectors->reset();
    CHECK(!lifetime.expired(), "retiring one producer cannot close another producer's store");
    bool missing = false;
    const auto read = io.Post([&] {
      const std::string key(64, 'a');
      const auto first = (*buildings)->Load(key);
      const auto second = (*roads)->LoadKey(key, 1024);
      missing = first && !*first && second && !*second;
    });
    CHECK(read != Tasks::kNoTask, "the shared store accepts actual worker IO");
    if (read != Tasks::kNoTask) { io.Wait(read); }
    CHECK(missing && admission->Active() == 1 && std::filesystem::exists(directory.Path),
          "different product families read through one admitted store after caller retirement");
  }
  const auto retired = compute.Post([] {});
  CHECK(retired != Tasks::kNoTask,
        "deferred producer cleanup is drained before checking ownership");
  if (retired != Tasks::kNoTask) { compute.Wait(retired); }
  const auto closed = io.Post([] {});
  CHECK(closed != Tasks::kNoTask, "the store close queue is drained after producer retirement");
  if (closed != Tasks::kNoTask) { io.Wait(closed); }
  CHECK(lifetime.expired() && admission->Active() == 0,
        "the last producer releases the store and its actual IO admission without a global owner");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::atomic_uint calls{0};
  UrbanAssetFixture::NoTransport wire;
  auto session = OsmCache::OpenSession(std::make_unique<UrbanAssetFixture::VectorSource>(
      calls, UrbanAssetFixture::Availability::Absent));
  CHECK(session, "shared-store producers receive an owned source session");
  if (!session) { return Report(); }
  Tasks compute(1);
  RejectMissingStore(*session, wire, compute);
  SharedLifetime(*session, wire);
  CHECK(calls == 0, "store binding and cached metadata reads never acquire provider payloads");
  return Report();
}
