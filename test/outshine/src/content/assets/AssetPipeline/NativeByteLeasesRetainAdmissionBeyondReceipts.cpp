#include "AssetPipeline.h"
#include "ByteLease.h"
#include "OpenAssetStore.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytes = 8192;
const std::string kKey(64, 'a');

AssetRecord Record() {
  return {.Key = kKey,
          .Kind = "leased-native",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = {},
          .OffsetBytes = 64,
          .ByteCount = 128,
          .Parent = {}};
}

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
  const auto directory = PlantedPath("native-byte-lease-admission");
  std::filesystem::remove_all(directory);
  auto cache = OpenAssetStore(directory);
  CHECK(cache, "the real native store opens");
  if (!cache) { return Report(); }
  const std::vector<uint8_t> payload(kBytes, 37);
  const std::array records{Record()};
  CHECK(cache->Publish(records, payload), "a subrange lives in a complete native package");
  const auto plan = cache->PlanRead(kKey, kBytes);
  CHECK(plan && *plan, "native metadata derives the actual read working bound");
  if (!plan || !*plan) { return Report(); }
  {
    auto direct = cache->Load(kKey, **plan);
    CHECK(direct && *direct, "a direct legacy read is available outside shared admission");
    if (!direct || !*direct) { return Report(); }
    AssetPublication unadmitted;
    unadmitted.Status = AssetPublicationStatus::Ready;
    unadmitted.Native = std::make_shared<CachedAsset>(std::move(**direct));
    CHECK(!unadmitted.Lease(), "an unadmitted raw product cannot create a Base lease");
  }
  auto metadata = Record();
  metadata.Package = std::string(64, 'b');
  const auto receiptBytes = AssetProduct(std::move(metadata)).CapacityBytes();
  const auto key = AssetProductKey::Parse(kKey);
  CHECK(key, "the native content identifier is valid");
  if (!key) { return Report(); }
  std::optional<ByteLease> survivor;
  {
    Tasks pool(1);
    AssetPipeline pipeline(pool,
                           {.JobsMost = 1,
                            .ConsumersMost = 1,
                            .TicketsMost = 1,
                            .InputBytesMost = (**plan).BufferBytesMost,
                            .ReceiptBytesMost = receiptBytes});
    auto consumer = pipeline.OpenConsumer({.Identity = cache});
    CHECK(consumer, "the consumer uses the real store scope");
    if (!consumer) { return Report(); }
    auto ticket = pipeline.RequestResolved(
        *consumer,
        {.Key = *key,
         .Generation = consumer->Generation(),
         .InputBytes = (**plan).BufferBytesMost,
         .ReceiptBytesMost = receiptBytes},
        [cache,
         readPlan = **plan](const std::stop_token &) -> std::expected<AssetProduct, std::string> {
          auto native = cache->Load(kKey, readPlan);
          if (!native || !*native) { return std::unexpected("native product failed to load"); }
          return AssetProduct(std::move(**native));
        });
    CHECK(ticket, "the current read plan admits production before native body IO");
    if (!ticket) { return Report(); }
    auto publication = Wait(*ticket);
    CHECK(publication.Status == AssetPublicationStatus::Ready && publication.Native,
          "the native product publishes without replacing its backing");
    if (!publication.Native) { return Report(); }
    survivor = publication.Lease();
    CHECK(survivor && survivor->Bytes().size() == payload.size() &&
              publication.Native->Bytes().data() == survivor->Bytes().data() + Record().OffsetBytes,
          "the generic lease aliases the complete package behind the record subrange");
    publication = {};
    ticket = std::unexpected(AssetPublicationError::Cancelled);
    CHECK(survivor && pipeline.Counts().Jobs == 0 &&
              pipeline.Counts().InputBytes == survivor->CapacityBytes() &&
              pipeline.Counts().ReceiptBytes == receiptBytes,
          "the Base lease holds backing and metadata admission after all request receipts retire");
  }
  cache.reset();
  std::filesystem::remove_all(directory);
  CHECK(survivor && std::ranges::equal(survivor->Bytes(), payload),
        "generic bytes survive pipeline, worker pool, source store and backing file removal");
  survivor.reset();
  Covers("real native subrange, zero-copy Base lease, retained admission and post-dispatcher "
         "lifetime");
  return Report();
}
