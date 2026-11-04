#include "GeometryAssetRequest.h"
#include "BuildingGeometrySource.h"
#include "PreparedBuildingAssets.h"
#include "BuildingAssetFixture.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators;
constexpr size_t kBytes = size_t{1024} * 1024;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr GeometryAssetRequest::Budget kBudget{.ProducerBytes = kBytes,
                                               .ConsumerBytes = kBytes,
                                               .EncodedBytesMost = kBytes,
                                               .ReceiptBytesMost = 4096};

struct Hold {
  std::binary_semaphore Started{0};
  std::binary_semaphore Proceed{0};

  bool Wait() {
    Started.release();
    return Proceed.try_acquire_for(kDeadline);
  }
};

struct Fixture {
  Tasks Workers{1};
  AssetPipeline Pipeline{Workers,
                         {.JobsMost = 4,
                          .ConsumersMost = 8,
                          .TicketsMost = 8,
                          .InputBytesMost = 8 * kBytes,
                          .ReceiptBytesMost = 16384}};
  Data::RawSourceCache Raw;
  Data::SourceSet Sources{Raw};
  std::shared_ptr<AssetCache> Cache;
  std::shared_ptr<PreparedBuildingAssets> Assets;
  BuildingGeometrySource::Recipe Recipe;
  std::atomic_int Generated{0};
  std::atomic_int Prepared{0};
  const std::thread::id Frame = std::this_thread::get_id();

  explicit Fixture(const std::filesystem::path &root)
      : Raw({.Directory = (root / "raw").string()}) {
    Cache = OpenAssetStore((root / "products").string());
    auto opened =
        PreparedBuildingAssets::Open(OpenAssetStore((root / "prepared").string()), Sources);
    CHECK(opened, "the native generator owns a separate prepared store");
    if (!opened) { return; }
    Assets = *opened;
    const auto heights = BuildingAssetFixture::Heights();
    CHECK(heights, "the generator fixture has terrain");
    if (!heights) { return; }
    Recipe.BaseKey = Assets->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000, std::string(64, 'a'));
    Recipe.SourceCount = 1;
    Recipe.Sources = {0};
    const std::atomic_bool stop{false};
    const auto base =
        Assets->Generate(Recipe.BaseKey, BuildingAssetFixture::Inputs(), *heights, stop);
    CHECK(base && *base, "one native building source prepares");
  }

  std::expected<GeometryAssetRequest, AssetPublicationError> Start(
      std::function<std::expected<void, std::string>(const GeometryAsset::Product &)> prepare = {},
      bool cacheOnly = false) {
    auto budget = kBudget;
    if (cacheOnly) { budget.ProducerBytes = 0; }
    return GeometryAssetRequest::Start(
        Pipeline,
        Workers,
        Cache,
        Recipe.Key(),
        Recipe.BaseKey,
        budget,
        [this](const std::stop_token &stop) -> std::expected<void, std::string> {
          CHECK(std::this_thread::get_id() != Frame,
                "native generation never runs on the frame thread");
          ++Generated;
          auto generated = BuildingGeometrySource::Resolve(
              *Cache,
              Assets.get(),
              Recipe,
              {.EncodedBytesMost = kBytes, .TriangleAttempts = 100000},
              stop);
          if (!generated) { return std::unexpected(generated.error()); }
          return {};
        },
        [this, prepare = std::move(prepare)](GeometryAsset::Product &product)
            -> std::expected<GeometryAssetRequest::Prepared, std::string> {
          CHECK(std::this_thread::get_id() != Frame, "decode preparation stays on the worker");
          ++Prepared;
          if (prepare) {
            const auto prepared = prepare(product);
            if (!prepared) { return std::unexpected(prepared.error()); }
          }
          return GeometryAssetRequest::Prepared{};
        });
  }
};

std::expected<std::shared_ptr<const GeometryAsset::Product>, std::string>
Await(GeometryAssetRequest &request) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto result = request.Poll();
    if (!result || *result) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return std::unexpected("geometry request deadline");
}

void Joined(const std::filesystem::path &root) {
  Fixture fixture(root);
  const auto hold = std::make_shared<Hold>();
  CHECK(fixture.Workers.PostDetached([hold] { (void)hold->Wait(); }),
        "the worker can be held before IO");
  CHECK(hold->Started.try_acquire_for(kDeadline), "the controlled worker reaches its hold");
  auto first = fixture.Start();
  auto second = fixture.Start();
  CHECK(first && second, "two consumers join one immutable native recipe");
  if (!first || !second) {
    hold->Proceed.release();
    return;
  }
  std::optional<GeometryAssetRequest> a(std::move(*first));
  std::optional<GeometryAssetRequest> b(std::move(*second));
  const auto pending = a->Poll();
  CHECK(pending && !*pending && fixture.Generated == 0 && fixture.Prepared == 0,
        "polling a held worker performs neither IO nor decode on the frame");
  hold->Proceed.release();
  auto readyA = Await(*a);
  auto readyB = Await(*b);
  CHECK(readyA && *readyA && readyB && *readyB && fixture.Generated == 1 && fixture.Prepared == 2,
        "one native publication supports two independently prepared leases");
  if (!readyA || !*readyA || !readyB || !*readyB) { return; }
  CHECK((*readyA)->Layout.Coverage == (*readyB)->Layout.Coverage && (*readyA)->Mesh.parts() > 0,
        "both consumers retain complete native geometry and source coverage");
  const size_t retained = fixture.Pipeline.Counts().InputBytes;
  CHECK(retained > 0 && retained < 2 * kBytes, "completed reads release unused decoder workspace");
  a.reset();
  b.reset();
  CHECK(fixture.Pipeline.Counts().InputBytes == retained,
        "destroying requests leaves both decoded product reservations alive");
  readyA->reset();
  CHECK(fixture.Pipeline.Counts().InputBytes == retained / 2,
        "one released product frees only its own reservation");
  readyB->reset();
  CHECK(fixture.Pipeline.Counts().InputBytes == 0 && fixture.Pipeline.Counts().Tickets == 0,
        "the last decoded lease frees all retained input admission");
  auto hit = fixture.Start({}, true);
  CHECK(hit, "a cache-only native request needs no generation reservation");
  if (!hit) { return; }
  const auto cached = Await(*hit);
  CHECK(cached && *cached && fixture.Generated == 1,
        "a persisted hit bypasses its native publisher entirely");
}

void PreparedBuffers(Fixture &fixture) {
  const size_t before = fixture.Pipeline.Counts().InputBytes;
  auto buffers = std::make_shared<std::vector<std::byte>>();
  const std::weak_ptr<const void> owned = buffers;
  auto budget = kBudget;
  budget.ProducerBytes = 0;
  auto requested = GeometryAssetRequest::Start(
      fixture.Pipeline,
      fixture.Workers,
      fixture.Cache,
      fixture.Recipe.Key(),
      fixture.Recipe.BaseKey,
      budget,
      {},
      [buffers](GeometryAsset::Product &product)
          -> std::expected<GeometryAssetRequest::Prepared, std::string> {
        buffers->resize(8192);
        product.Mesh.clear();
        return GeometryAssetRequest::Prepared{
            .Owner = buffers, .Bytes = sizeof(std::vector<std::byte>) + buffers->capacity()};
      });
  CHECK(requested, "a native consumer can replace decoded meshes with prepared buffers");
  if (!requested) { return; }
  std::optional<GeometryAssetRequest> request(std::move(*requested));
  auto ready = Await(*request);
  CHECK(ready && *ready && (*ready)->Mesh.parts() == 0,
        "only the consumer's prepared form remains after the worker consumes the mesh");
  if (!ready || !*ready) { return; }
  const size_t retained = fixture.Pipeline.Counts().InputBytes;
  CHECK(retained >= before + buffers->capacity() && retained < before + kBytes,
        "prepared buffer residency replaces the larger decoder workspace reservation");
  request.reset();
  CHECK(fixture.Pipeline.Counts().InputBytes == retained,
        "the prepared lease retains its reservation");
  buffers.reset();
  CHECK(!owned.expired(), "the decoded lease owns prepared buffers without external handles");
  ready->reset();
  CHECK(owned.expired() && fixture.Pipeline.Counts().InputBytes == before,
        "prepared buffers retire before their admission");
}

void EvictionBeforeDecode(const std::filesystem::path &root) {
  Fixture fixture(root);
  auto first = fixture.Start();
  CHECK(first, "the controlled native publication starts");
  if (!first) { return; }
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (fixture.Pipeline.Counts().Running != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(fixture.Pipeline.Counts().Running == 0, "the native receipt precedes delayed decode");
  const auto hold = std::make_shared<Hold>();
  CHECK(fixture.Workers.PostDetached([hold] { (void)hold->Wait(); }),
        "the decoder queue can be held");
  CHECK(hold->Started.try_acquire_for(kDeadline), "the worker stops before opening geometry");
  const auto pending = first->Poll();
  CHECK(pending && !*pending, "a published receipt starts no inline geometry load");
  CHECK(fixture.Cache->Remove(fixture.Recipe.Key()),
        "cache maintenance evicts the queued geometry");
  hold->Proceed.release();
  CHECK(!Await(*first), "an evicted body never becomes a successful empty product");
  first = std::unexpected(AssetPublicationError::Rejected);
  auto retry = fixture.Start();
  CHECK(retry, "a rejected stale receipt permits a new native request");
  if (!retry) { return; }
  const auto restored = Await(*retry);
  CHECK(restored && *restored && fixture.Generated == 2,
        "the missing body is regenerated instead of reusing a stale ready receipt");
}

void CancellationAndLocalFailure(const std::filesystem::path &root) {
  Fixture fixture(root);
  auto initial = fixture.Start();
  CHECK(initial, "the native miss starts");
  if (!initial) { return; }
  auto ready = Await(*initial);
  CHECK(ready && *ready, "the immutable geometry precedes cancellation cases");
  if (!ready || !*ready) { return; }
  ready->reset();
  initial = std::unexpected(AssetPublicationError::Cancelled);
  const auto hold = std::make_shared<Hold>();
  auto requested = fixture.Start(
      [hold](const GeometryAsset::Product &) -> std::expected<void, std::string> {
        if (!hold->Wait()) { return std::unexpected("preparation hold expired"); }
        return {};
      },
      true);
  CHECK(requested, "the controlled cache decoder starts");
  if (!requested) { return; }
  std::optional<GeometryAssetRequest> pending(std::move(*requested));
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  bool started = false;
  while (!(started = hold->Started.try_acquire()) && std::chrono::steady_clock::now() < until) {
    const auto polled = pending->Poll();
    CHECK(polled, "the worker preparation remains pending");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(started, "the decoder reaches its held worker preparation");
  pending->Cancel();
  CHECK(!pending->Poll(), "cancellation prevents a new decoded lease");
  pending.reset();
  CHECK(fixture.Pipeline.Counts().InputBytes == kBytes,
        "cancelled in-flight preparation keeps its reservation until worker exit");
  hold->Proceed.release();
  const auto released = std::chrono::steady_clock::now() + kDeadline;
  while (fixture.Pipeline.Counts().InputBytes != 0 && std::chrono::steady_clock::now() < released) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(fixture.Pipeline.Counts().InputBytes == 0,
        "worker exit releases cancelled geometry admission");
  auto failed = fixture.Start(
      [](const GeometryAsset::Product &) -> std::expected<void, std::string> {
        return std::unexpected("local preparation failure");
      },
      true);
  auto valid = fixture.Start({}, true);
  CHECK(failed && valid, "two consumers may require different local preparation");
  if (!failed || !valid) { return; }
  CHECK(!Await(*failed), "a local preparation failure reaches only its consumer");
  const auto good = Await(*valid);
  CHECK(good && *good && fixture.Generated == 1,
        "local failure preserves the other consumer and the valid native publication");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-tests.async-native-geometry-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Joined(root / "joined");
  CancellationAndLocalFailure(root / "cancel");
  EvictionBeforeDecode(root / "eviction");
  {
    Fixture fixture(root / "buffers");
    auto initial = fixture.Start();
    CHECK(initial, "the prepared-buffer test starts its native product");
    if (initial) {
      const auto ready = Await(*initial);
      CHECK(ready && *ready, "the native asset is cached");
    }
    PreparedBuffers(fixture);
  }
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
