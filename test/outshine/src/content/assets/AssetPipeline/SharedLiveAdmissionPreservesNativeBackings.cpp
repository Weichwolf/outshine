#include "AssetCache.h"
#include "AssetPipeline.h"
#include "AssetProduct.h"
#include "ByteLease.h"
#include "Check.h"
#include "OpenAssetStore.h"
#include "Tasks.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <memory>
#include <semaphore>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::string kKey(64, 'b');

struct NativeReader {
  std::shared_ptr<AssetCache> Cache;
  AssetCache::ReadPlan Plan;
  std::atomic_size_t Reads{0};
  std::binary_semaphore Entered{0};
  std::binary_semaphore Finish{0};

  std::expected<AssetProduct, std::string> Read() {
    const auto number = ++Reads;
    auto native = Cache->Load(kKey, Plan);
    if (!native || !*native) { return std::unexpected("native body read failed"); }
    if (number == 1) {
      Entered.release();
      if (!Finish.try_acquire_for(std::chrono::seconds(5))) {
        return std::unexpected("native publication barrier timed out");
      }
    }
    return AssetProduct(std::move(**native));
  }
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
}

int main() {
  const auto directory = PlantedPath("shared-live-native-admission");
  std::filesystem::remove_all(directory);
  auto cache = OpenAssetStore(directory);
  CHECK(cache, "the native asset store opens");
  if (!cache) { return Report(); }
  const std::vector<uint8_t> payload(4096, 57);
  AssetRecord record{.Key = kKey,
                     .Kind = "shared-live-native",
                     .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                     .Package = {},
                     .ByteCount = payload.size(),
                     .Parent = {}};
  CHECK(cache->Publish(std::array{record}, payload), "the complete immutable asset is stored");
  const auto plan = cache->PlanRead(kKey, payload.size());
  const auto key = AssetProductKey::Parse(kKey);
  CHECK(plan && *plan && key, "the real metadata yields a native read plan and content key");
  if (!plan || !*plan || !key) { return Report(); }
  record.Package = std::string(64, 'a');
  record.RequestKey = std::string(64, 'c');
  const auto metadata = AssetProduct(std::move(record)).CapacityBytes();
  const auto bound = (**plan).BufferBytesMost;
  MemoryBudget memory(bound);
  std::optional<ByteLease> survivor;
  {
    const auto reader = std::make_shared<NativeReader>();
    reader->Cache = cache;
    reader->Plan = **plan;
    Tasks pool(1);
    const AssetPipeline::Limits limits{.JobsMost = 1,
                                       .ConsumersMost = 1,
                                       .TicketsMost = 1,
                                       .InputBytesMost = bound,
                                       .ReceiptBytesMost = metadata};
    AssetPipeline first(pool, limits, memory);
    AssetPipeline second(pool, limits, memory);
    auto owner = first.OpenConsumer({.Identity = cache});
    auto other = second.OpenConsumer({.Identity = cache});
    CHECK(owner && other, "independent producers consume the same external admission account");
    if (!owner || !other) { return Report(); }
    const auto read = [reader](const std::stop_token &) { return reader->Read(); };
    auto loaded = first.RequestResolved(*owner,
                                        {.Key = *key,
                                         .Generation = owner->Generation(),
                                         .InputBytes = bound,
                                         .ReceiptBytesMost = metadata},
                                        read);
    CHECK(loaded, "the first native read is admitted before body IO");
    if (!loaded) { return Report(); }
    CHECK(reader->Entered.try_acquire_for(std::chrono::seconds(5)),
          "the worker holds an admitted native body before publication");
    memory.SetMaximum(0);
    CHECK(memory.Used() == bound && memory.Available() == 0,
          "runtime pressure preserves every byte held by unfinished work");
    reader->Finish.release();
    auto publication = Wait(*loaded);
    survivor = publication.Lease();
    CHECK(survivor && reader->Reads == 1 && memory.Used() == survivor->CapacityBytes(),
          "only the live backing remains booked after read scratch is released");
    if (!survivor) { return Report(); }
    publication = {};
    loaded = std::unexpected(AssetPublicationError::Cancelled);
    memory.SetMaximum(0);
    std::atomic_size_t calls{0};
    auto pending = second.RequestLogical(
        *other,
        {.Address = std::string(64, 'c'),
         .Generation = other->Generation(),
         .ReceiptBytesMost = metadata},
        [&, read](const std::stop_token &stop)
            -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
          if (++calls == 1) { return AssetPipeline::InputAdmission{.Bytes = bound}; }
          auto native = read(stop);
          if (!native) { return std::unexpected(native.error()); }
          native->SetRequestBinding(std::string(64, 'c'));
          return std::optional<AssetProduct>(std::move(*native));
        });
    CHECK(pending, "zero-input discovery remains possible while positive admission is suspended");
    if (!pending) { return Report(); }
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (calls == 0 && std::chrono::steady_clock::now() < until) { std::this_thread::yield(); }
    for (size_t attempt = 0; attempt < 16; ++attempt) {
      const auto result = pending->Poll();
      CHECK(!result.Finished, "temporary runtime pressure does not invalidate a valid work plan");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(calls == 1 && reader->Reads == 1 && memory.Available() == 0 &&
              std::ranges::equal(survivor->Bytes(), payload),
          "the second producer cannot enter IO or allocate through unsigned room after reduction");
    memory.SetMaximum(memory.Used() + bound);
    const auto complete = Wait(*pending);
    auto next = complete.Lease();
    CHECK(complete.Status == AssetPublicationStatus::Ready && next && reader->Reads == 2 &&
              calls == 2 && std::ranges::equal(next->Bytes(), payload),
          "raising shared admission resumes the original job and preserves native content");
    memory.SetMaximum(0);
  }
  cache.reset();
  std::filesystem::remove_all(directory);
  CHECK(survivor && memory.Used() == survivor->CapacityBytes() && memory.Available() == 0 &&
            std::ranges::equal(survivor->Bytes(), payload),
        "a native lease outlives both pipelines, their pool and the store after admission closes");
  survivor.reset();
  CHECK(memory.Used() == 0, "the final immutable backing repays the shared account exactly once");
  Covers("real native IO, shared live admission, paused growth and last backing; not physical UMA");
  return Report();
}
