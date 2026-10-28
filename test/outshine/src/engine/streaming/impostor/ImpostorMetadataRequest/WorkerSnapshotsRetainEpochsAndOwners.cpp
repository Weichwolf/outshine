#include "ImpostorMetadataRequest.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <expected>
#include <memory>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Metadata = Content::ImpostorViewAssets;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr std::string_view kProvenance = "metadata/request-1";

struct Fixture {
  Tasks Workers{1};
  AssetPipeline Pipeline;
  std::shared_ptr<AssetCache> Cache;
  std::optional<AssetPipeline::Consumer> Consumer;

  explicit Fixture(const std::filesystem::path &path, size_t inputBytes = 65536)
      : Pipeline(Workers,
                 {.JobsMost = 2,
                  .ConsumersMost = 2,
                  .TicketsMost = 4,
                  .InputBytesMost = inputBytes,
                  .ReceiptBytesMost = 16384}) {
    auto cache = AssetCache::Open(path.string());
    CHECK(cache, "the metadata store opens");
    if (!cache) { return; }
    Cache = std::move(*cache);
    auto consumer = Pipeline.OpenConsumer({.Identity = Cache});
    CHECK(consumer, "the metadata store owns its consumer scope");
    if (consumer) { Consumer.emplace(std::move(*consumer)); }
  }

  [[nodiscard]] std::expected<Metadata, std::string> Publish(double extent = 1) const {
    auto result = Metadata::Publish(
        *Cache,
        kProvenance,
        {.Centre = {}, .HalfExtentM = extent, .Pixels = 8, .Directions = {{{0, 0, 1}}}},
        {.Min = {{-2, -2, -2}}, .Max = {{2, 2, 2}}});
    if (!result) { return std::unexpected("fixture metadata publication failed"); }
    return std::move(*result);
  }
};

template <class Predicate> bool Await(Predicate predicate) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    if (predicate()) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

std::expected<std::shared_ptr<const Metadata>, std::string>
Result(ImpostorMetadataRequest &request) {
  std::expected<std::shared_ptr<const Metadata>, std::string> result;
  const bool complete = Await([&] {
    result = request.Poll();
    return !result || *result != nullptr;
  });
  if (!complete) { return std::unexpected("metadata fixture deadline expired"); }
  return result;
}

void BackgroundLease(const std::filesystem::path &path) {
  Fixture fixture(path);
  if (!fixture.Consumer) { return; }
  std::shared_ptr<const Metadata> retained;
  const auto mainThread = std::this_thread::get_id();
  std::atomic<bool> background{false};
  auto source = std::make_shared<int>(7);
  const std::weak_ptr<int> sourceOwner = source;
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  const auto blocked = fixture.Workers.Post([&] {
    entered.release();
    release.acquire();
  });
  CHECK(entered.try_acquire_for(kDeadline), "the worker is held before metadata work");
  {
    auto request = ImpostorMetadataRequest::Start(
        fixture.Pipeline,
        *fixture.Consumer,
        fixture.Workers,
        fixture.Cache,
        std::string(kProvenance),
        [&, source = std::move(source)](const std::stop_token &) {
          background.store(std::this_thread::get_id() != mainThread && *source == 7);
          return fixture.Publish();
        },
        64);
    CHECK(request, "metadata production is admitted before any caller-side IO");
    if (!request) {
      release.release();
      fixture.Workers.Wait(blocked);
      return;
    }
    const auto pending = request->Poll();
    CHECK(pending && !*pending && !background.load() && !sourceOwner.expired(),
          "Start and Poll leave held source work pending with its owner intact");
    release.release();
    fixture.Workers.Wait(blocked);
    auto moved = std::move(*request);
    auto target = ImpostorMetadataRequest::Start(
        fixture.Pipeline,
        *fixture.Consumer,
        fixture.Workers,
        fixture.Cache,
        std::string(kProvenance),
        [&](const std::stop_token &) { return fixture.Publish(); },
        64);
    CHECK(target, "a replaceable request retains its own publication demand");
    if (!target) { return; }
    *target = std::move(moved);
    CHECK(fixture.Pipeline.Counts().Tickets == 1,
          "move assignment releases the previous destination demand");
    const auto result = Result(*target);
    CHECK(result && *result && background.load(), "the owned source runs only on the worker");
    CHECK(sourceOwner.expired(), "completed publication does not retain the source factory");
    if (result) { retained = *result; }
    CHECK(fixture.Consumer->Advance({2}) && !target->Poll(),
          "advancing the World generation also invalidates an already decoded snapshot");
  }
  CHECK(retained && retained->Metadata().Pixels == 8 && fixture.Pipeline.Counts().Tickets == 1,
        "the CPU snapshot retains its cache and publication after request destruction");
  retained.reset();
  CHECK(Await([&] { return fixture.Pipeline.Counts().Tickets == 0; }),
        "the final CPU consumer releases its publication reservation");
}

void SharedCancellation(const std::filesystem::path &path) {
  Fixture fixture(path);
  if (!fixture.Consumer) { return; }
  auto other = fixture.Pipeline.OpenConsumer({.Identity = fixture.Cache});
  CHECK(other, "a second independently cancellable consumer opens");
  if (!other) { return; }
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  const auto blocked = fixture.Workers.Post([&] {
    entered.release();
    release.acquire();
  });
  CHECK(entered.try_acquire_for(kDeadline), "shared publication waits behind the held worker");
  std::atomic<size_t> firstCalls{0};
  std::atomic<size_t> secondCalls{0};
  auto first = ImpostorMetadataRequest::Start(
      fixture.Pipeline,
      *fixture.Consumer,
      fixture.Workers,
      fixture.Cache,
      std::string(kProvenance),
      [&](const std::stop_token &) {
        ++firstCalls;
        return fixture.Publish();
      },
      64);
  auto second = ImpostorMetadataRequest::Start(
      fixture.Pipeline,
      *other,
      fixture.Workers,
      fixture.Cache,
      std::string(kProvenance),
      [&](const std::stop_token &) {
        ++secondCalls;
        return fixture.Publish();
      },
      64);
  CHECK(first && second, "same-store metadata requests share one admitted producer");
  if (first) { first->Cancel(); }
  release.release();
  fixture.Workers.Wait(blocked);
  if (!first || !second) { return; }
  const auto result = Result(*second);
  CHECK(!first->Poll() && result && *result && firstCalls == 1 && secondCalls == 0,
        "one cancellation preserves the other consumer and the original source factory");
}

void StaleRead(const std::filesystem::path &path, bool changeGeneration) {
  Fixture fixture(path);
  if (!fixture.Consumer) { return; }
  auto request = ImpostorMetadataRequest::Start(
      fixture.Pipeline,
      *fixture.Consumer,
      fixture.Workers,
      fixture.Cache,
      std::string(kProvenance),
      [&](const std::stop_token &) { return fixture.Publish(); },
      64);
  CHECK(request && Await([&] { return fixture.Pipeline.Counts().Running == 0; }),
        "the metadata receipt precedes the separately queued CPU read");
  if (!request) { return; }
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  const auto blocked = fixture.Workers.Post([&] {
    entered.release();
    release.acquire();
  });
  CHECK(entered.try_acquire_for(kDeadline), "the metadata reader is held before decoding");
  const auto pending = request->Poll();
  CHECK(pending && !*pending, "Poll does not decode a ready receipt on the caller");
  if (changeGeneration) {
    CHECK(fixture.Consumer->Advance({2}), "the World consumer advances its generation");
  } else {
    CHECK(fixture.Publish(2), "a changed metadata package replaces the pending binding");
  }
  release.release();
  fixture.Workers.Wait(blocked);
  CHECK(!Result(*request), "a stale generation or metadata package cannot publish a CPU snapshot");
}

void Admission(const std::filesystem::path &path) {
  Fixture fixture(path, 1);
  if (!fixture.Consumer) { return; }
  std::atomic<size_t> calls{0};
  const auto request = ImpostorMetadataRequest::Start(
      fixture.Pipeline,
      *fixture.Consumer,
      fixture.Workers,
      fixture.Cache,
      std::string(kProvenance),
      [&](const std::stop_token &) {
        ++calls;
        return fixture.Publish();
      },
      64);
  CHECK(!request && request.error() == AssetPublicationError::ResourceDeferred && calls == 0,
        "bounded pipeline admission precedes source and cache work");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-metadata-request-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  BackgroundLease(root / "lease.sqlite");
  SharedCancellation(root / "shared.sqlite");
  StaleRead(root / "generation.sqlite", true);
  StaleRead(root / "package.sqlite", false);
  Admission(root / "admission.sqlite");
  std::filesystem::remove_all(root);
  return Report();
}
