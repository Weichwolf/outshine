#include "LitterFieldPreparation.h"
#include "Check.h"
#include "Tasks.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <semaphore>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr LitterFieldAddress kAddress{.Tile = {-65, -64}, .Level = 4};

AssetPipeline::Limits Limits(size_t inputBytes = 16u << 20u) {
  return {.JobsMost = 4,
          .ConsumersMost = 4,
          .TicketsMost = 8,
          .InputBytesMost = inputBytes,
          .ReceiptBytesMost = 8192};
}

class WriteLock {
public:
  explicit WriteLock(const std::string &path) {
    sqlite3 *raw = nullptr;
    CHECK(sqlite3_open(path.c_str(), &raw) == SQLITE_OK, "an independent writer opens");
    Database_.reset(raw);
    if (Database_) {
      CHECK(sqlite3_exec(Database_.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) ==
                SQLITE_OK,
            "the independent writer holds the publication lock");
    }
  }

  ~WriteLock() { Release(); }

  WriteLock(const WriteLock &) = delete;
  WriteLock &operator=(const WriteLock &) = delete;

  explicit operator bool() const noexcept { return Database_ != nullptr; }

  void Release() {
    if (!Database_) { return; }
    CHECK(sqlite3_exec(Database_.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases its transaction");
    CHECK(sqlite3_close(Database_.release()) == SQLITE_OK, "the independent writer closes");
  }

private:
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Database_{nullptr, sqlite3_close};
};

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

void SharedDemand(const std::shared_ptr<AssetCache> &cache) {
  Tasks workers(1);
  AssetPipeline pipeline(workers, Limits());
  auto a = pipeline.OpenConsumer({.Identity = cache});
  auto b = pipeline.OpenConsumer({.Identity = cache});
  CHECK(a && b, "two consumers open in the same native asset store");
  if (!a || !b) { return; }
  std::binary_semaphore occupied(0);
  std::binary_semaphore release(0);
  const auto blocker = workers.Post([&] {
    occupied.release();
    release.acquire();
  });
  CHECK(blocker != Tasks::kNoTask, "one worker is occupied before submitting demand");
  if (blocker == Tasks::kNoTask) { return; }
  const bool started = occupied.try_acquire_for(std::chrono::seconds(5));
  CHECK(started, "the worker starts within the test deadline");
  if (!started) {
    release.release();
    return;
  }
  auto first = RequestLitterField(pipeline, *a, cache, kAddress);
  auto second = RequestLitterField(pipeline, *b, cache, kAddress);
  const auto pending = pipeline.Counts();
  CHECK(first && second && pending.Jobs == 1 && pending.InputBytes == 0,
        "equal selected mips share a job without caller-side IO or image allocation");
  CHECK(a->Advance({.Value = 2}), "one camera demand changes generation independently");
  release.release();
  CHECK(workers.AwaitCompletion(blocker, 5), "the occupied worker is released");
  (void)workers.TakeCompletion(blocker);
  if (!first || !second) { return; }
  const auto result = Wait(*second);
  const auto cancelled = Wait(*first);
  CHECK(result.Status == AssetPublicationStatus::Ready && result.Native && result.Receipt &&
            result.Native->Bytes().size() == 1040 && !result.RequestHit,
        "the surviving demand receives only the selected coarse native image");
  CHECK(cancelled.Status == AssetPublicationStatus::Cancelled,
        "changing one generation does not cancel the shared surviving demand");
  if (result.Native) {
    const auto view = DecodeLitterField(kAddress, result.Native->Bytes());
    CHECK(view && view->Pixels.size() == 1024,
          "ready backing directly decodes to the requested R8 pixels");
  }
  auto fine = LitterFieldKey({.Tile = kAddress.Tile, .Level = 0});
  auto found = fine ? cache->Find(*fine) : std::unexpected(AssetCacheError::InvalidInput);
  CHECK(found && !*found, "coarse demand neither generates nor stores the finest sibling");
}

void ReadOnlyAndBudget(const std::shared_ptr<AssetCache> &cache, const std::string &path) {
  Tasks workers(1);
  AssetPipeline pipeline(workers, Limits());
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  CHECK(consumer, "offline consumer opens");
  if (!consumer) { return; }
  WriteLock writer(path);
  if (!writer) { return; }
  auto hit =
      RequestLitterField(pipeline, *consumer, cache, kAddress, LitterFieldMiss::Refuse, 2048);
  CHECK(hit, "a cached mip is requested without permitting its generator");
  if (hit) {
    const auto result = Wait(*hit);
    CHECK(result.Status == AssetPublicationStatus::Ready && result.Native &&
              result.ConsumerBytesMost == 2048 && result.RequestHit,
          "a native hit requires no write slot and carries admitted consumer workspace");
  }
  writer.Release();
  auto absent = RequestLitterField(
      pipeline, *consumer, cache, {.Tile = {12, 34}, .Level = 4}, LitterFieldMiss::Refuse);
  CHECK(absent && Wait(*absent).Status == AssetPublicationStatus::Failed,
        "a missing read-only product does not invoke generation");
  AssetPipeline limited(workers, Limits(16));
  auto small = limited.OpenConsumer({.Identity = cache});
  CHECK(small, "the low-budget consumer opens");
  if (!small) { return; }
  constexpr LitterFieldAddress missing{.Tile = {56, 78}, .Level = 4};
  auto denied = RequestLitterField(limited, *small, cache, missing);
  const auto result = denied ? Wait(*denied) : AssetPublication{};
  CHECK(denied && result.Status == AssetPublicationStatus::Failed &&
            result.Error == AssetPublicationError::InvalidRequest,
        "a request larger than the configured hard bound is refused before generation");
  const auto key = LitterFieldKey(missing);
  const auto stored = key ? cache->Find(*key) : std::unexpected(AssetCacheError::InvalidInput);
  CHECK(stored && !*stored, "denied generation leaves no native asset");
}

enum class Completion { Publish, Cancel };

void BusyPublication(const std::shared_ptr<AssetCache> &cache,
                     const std::string &path,
                     Completion finish) {
  Tasks workers(1);
  AssetPipeline pipeline(workers, Limits());
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  CHECK(consumer, "publication consumer opens");
  if (!consumer) { return; }
  const LitterFieldAddress address{.Tile = {finish == Completion::Cancel ? 235 : 234, -567},
                                   .Level = 4};
  auto package = PrepareLitterField(address, 1040);
  CHECK(package, "the bounded publication size reference is prepared");
  if (!package) { return; }
  auto prepared = cache->PreparePublication(std::move(package->Records), std::move(package->Bytes));
  CHECK(prepared, "the native codec provides the retained publication byte bound");
  if (!prepared) { return; }
  const auto heldBytes = prepared->CapacityBytes();
  WriteLock writer(path);
  if (!writer) { return; }
  auto ticket = RequestLitterField(pipeline, *consumer, cache, address);
  CHECK(ticket, "a cache miss enters the existing worker pipeline");
  if (!ticket) { return; }
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (pipeline.Counts().InputBytes != heldBytes && std::chrono::steady_clock::now() < until) {
    (void)ticket->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(pipeline.Counts().InputBytes == heldBytes,
        "busy publication retains only its prepared product rather than generation workspace");
  for (int attempt = 0; attempt < 10; ++attempt) {
    CHECK(!ticket->Poll().Finished, "the held writer defers the product without publishing");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const auto barrier = workers.Post([] {});
  CHECK(barrier != Tasks::kNoTask && workers.AwaitCompletion(barrier, 1),
        "busy publication releases the sole worker for unrelated tasks");
  if (barrier != Tasks::kNoTask) { (void)workers.TakeCompletion(barrier); }
  if (finish == Completion::Cancel) {
    ticket->Cancel();
    CHECK(Wait(*ticket).Status == AssetPublicationStatus::Cancelled,
          "cancelling a busy publication drops its demand before writer release");
    const auto stopped = workers.Post([] {});
    CHECK(stopped != Tasks::kNoTask && workers.AwaitCompletion(stopped, 1),
          "cancelled publication leaves no blocked worker");
    if (stopped != Tasks::kNoTask) { (void)workers.TakeCompletion(stopped); }
  }
  writer.Release();
  if (finish == Completion::Publish) {
    CHECK(Wait(*ticket).Status == AssetPublicationStatus::Ready,
          "the retained prepared image publishes and loads after writer release");
  }
  if (finish == Completion::Cancel) {
    const auto key = LitterFieldKey(address);
    const auto found = key ? cache->Find(*key) : std::unexpected(AssetCacheError::InvalidInput);
    CHECK(found && !*found, "a cancelled busy publication is not written after writer release");
  }
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-tests.litter-worker-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "private native database directory is available");
  if (error) { return Report(); }
  const auto path = (directory / "assets.sqlite").string();
  {
    auto opened = AssetCache::Open(path);
    CHECK(opened, "the native asset database opens");
    if (opened) {
      const std::shared_ptr<AssetCache> cache(std::move(*opened));
      SharedDemand(cache);
      ReadOnlyAndBudget(cache, path);
      BusyPublication(cache, path, Completion::Publish);
      BusyPublication(cache, path, Completion::Cancel);
    }
  }
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "private native products are removed after qualification");
  return Report();
}
