#include "AssetPipeline.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kPackageBytes = 8192, kReservation = 16384;
static_assert(!std::is_copy_constructible_v<CachedAsset>);
static_assert(!std::is_copy_assignable_v<CachedAsset>);
const std::string kContent(64, 'a'), kAddress(64, 'b'), kOtherAddress(64, 'c');

AssetRecord Record(char key, uint64_t offset, uint64_t count) {
  return {.Key = std::string(64, key),
          .Kind = "native-backing-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = {},
          .OffsetBytes = offset,
          .ByteCount = count,
          .Parent = {}};
}

struct StoreFixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-engine-native-backing-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::shared_ptr<AssetCache> Cache;
  std::vector<uint8_t> Payload = std::vector<uint8_t>(kPackageBytes);

  bool Open() {
    std::error_code error;
    std::filesystem::create_directories(Directory, error);
    if (error) { return false; }
    auto cache = AssetCache::Open((Directory / "assets.sqlite").string());
    if (!cache) { return false; }
    Cache = std::move(*cache);
    for (size_t at = 0; at < Payload.size(); ++at) {
      Payload[at] = static_cast<uint8_t>((at * 31 + 7) & 255);
    }
    std::array records{Record('a', 0, 32), Record('d', 32, kPackageBytes - 32)};
    records[0].RequestKey = kAddress;
    return Cache->Publish(records, Payload).has_value();
  }

  ~StoreFixture() {
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
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

struct Discovery {
  std::atomic<size_t> Queries{0}, Loads{0};
  std::atomic_bool Ready{false};
};

bool AwaitDeferredSharedContent(const AssetPipeline &pipeline,
                                const AssetPipeline::RequestTicket &first,
                                const AssetPipeline::RequestTicket &second,
                                const Discovery &control) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < until) {
    (void)first.Poll();
    (void)second.Poll();
    const auto held = pipeline.Counts();
    if (control.Queries.load() == 2 && held.Jobs == 1 && held.Running == 0) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

AssetPipeline::LogicalProducer Discover(std::shared_ptr<AssetCache> cache,
                                        const std::shared_ptr<Discovery> &control) {
  return [cache = std::move(cache), control](
             const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
    ++control->Queries;
    return AssetPipeline::ResolvedWork{
        .Key = *AssetProductKey::Parse(kContent),
        .Produce = [cache, control](const std::stop_token &)
            -> std::expected<std::optional<AssetProduct>, std::string> {
          if (!control->Ready.load()) { return std::nullopt; }
          auto loaded = cache->Load(kContent, kPackageBytes);
          if (!loaded || !*loaded) { return std::unexpected("native content did not load"); }
          ++control->Loads;
          return AssetProduct(std::move(**loaded));
        }};
  };
}

void SharedNativeResult() {
  StoreFixture fixture;
  CHECK(fixture.Open(), "two real records persist in one actual native package");
  if (!fixture.Cache) { return; }
  Tasks pool(2);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 4,
                          .InputBytesMost = 2 * kReservation,
                          .ReceiptBytesMost = 2 * kReservation});
  auto a = pipeline.OpenConsumer({.Identity = fixture.Cache});
  auto b = pipeline.OpenConsumer({.Identity = fixture.Cache});
  CHECK(a && b, "two consumers open in the same actual store scope");
  if (!a || !b) { return; }
  const auto control = std::make_shared<Discovery>();
  auto first = pipeline.RequestLogical(*a,
                                       {.Address = kAddress,
                                        .Generation = a->Generation(),
                                        .InputBytes = kReservation,
                                        .ReceiptBytesMost = kReservation},
                                       Discover(fixture.Cache, control));
  auto second = pipeline.RequestLogical(*b,
                                        {.Address = kOtherAddress,
                                         .Generation = b->Generation(),
                                         .InputBytes = kReservation,
                                         .ReceiptBytesMost = kReservation},
                                        Discover(fixture.Cache, control));
  CHECK(first && second, "both early addresses admit before native package IO");
  if (!first || !second) { return; }
  CHECK(AwaitDeferredSharedContent(pipeline, *first, *second, *control) &&
            control->Loads.load() == 0,
        "resolved addresses join one deferred content job before its single native load");
  control->Ready.store(true);
  auto left = Wait(*first);
  auto right = Wait(*second);
  CHECK(left.Status == AssetPublicationStatus::Ready && right.Status == left.Status &&
            left.Native && left.Native == right.Native && left.Receipt == right.Receipt &&
            control->Loads.load() == 1,
        "both logical consumers share one actual loaded backing and content receipt");
  if (!left.Native || !right.Native) { return; }
  CHECK(left.Receipt.get() == &left.Native->Record() && !left.Native->Record().RequestKey &&
            left.RequestKey == AssetRequestKey::Parse(kAddress) &&
            right.RequestKey == AssetRequestKey::Parse(kOtherAddress) &&
            std::ranges::equal(left.Native->Bytes(), std::span(fixture.Payload).first(32)),
        "content is unannotated while each ticket keeps its own address and exact native slice");
  auto native = left.Native;
  auto receipt = right.Receipt;
  first->Cancel();
  const auto cancelled = first->Poll();
  CHECK(cancelled.Status == AssetPublicationStatus::Cancelled && !cancelled.Receipt &&
            !cancelled.Native && second->Poll().Native == native,
        "cancelled demand cannot return native content while its live neighbor retains access");
  CHECK(b->Advance({.Value = 2}), "the other consumer advances beyond its completed generation");
  const auto stale = second->Poll();
  CHECK(stale.Status == AssetPublicationStatus::Cancelled && !stale.Native && !stale.Receipt &&
            stale.Error == AssetPublicationError::StaleGeneration,
        "stale completed demand cannot expose its prior native result");
  first = std::unexpected(AssetPublicationError::InvalidRequest);
  second = std::unexpected(AssetPublicationError::InvalidRequest);
  left = {};
  right = {};
  CHECK(pipeline.Counts().Jobs == 0 && pipeline.Counts().Tickets == 0 &&
            pipeline.Counts().InputBytes == kPackageBytes &&
            pipeline.Counts().ReceiptBytes == kReservation,
        "the shared native/record aliases retain one output reservation after job retirement");
  auto fallback = fixture.Cache->Find(std::string(64, 'd'));
  CHECK(fallback && *fallback, "the independent real content record is available");
  if (!fallback || !*fallback) { return; }
  const auto key = AssetProductKey::Parse((**fallback).Key);
  CHECK(key.has_value(), "the independent record retains its actual content digest");
  if (!key) { return; }
  const AssetPipeline::ResolvedRequest other{
      .Key = *key, .Generation = a->Generation(), .ReceiptBytesMost = 2 * kReservation};
  auto produce = [record = **fallback](const std::stop_token &) { return record; };
  auto blocked = pipeline.RequestResolved(*a, other, produce);
  CHECK(!blocked && blocked.error() == AssetPublicationError::ResourceDeferred,
        "terminal native storage participates in later output admission");
  auto ramDemand = other;
  ramDemand.InputBytes = 2 * kReservation;
  ramDemand.ReceiptBytesMost = kReservation;
  auto ramBlocked = pipeline.RequestResolved(*a, ramDemand, produce);
  CHECK(!ramBlocked && ramBlocked.error() == AssetPublicationError::ResourceDeferred,
        "retained native bytes independently consume the same RAM admission as new job inputs");
  receipt.reset();
  const std::weak_ptr<AssetCache> store = fixture.Cache;
  fixture.Cache.reset();
  CHECK(store.expired() && pipeline.Counts().ReceiptBytes == kReservation &&
            pipeline.Counts().InputBytes == kPackageBytes &&
            std::ranges::equal(native->Bytes(), std::span(fixture.Payload).first(32)),
        "the last native alias owns readable bytes and admission after the record and DB close");
  native.reset();
  CHECK(pipeline.Counts().ReceiptBytes == 0 && pipeline.Counts().InputBytes == 0,
        "last native release returns both its RAM ownership and metadata capacity");
  auto admitted = pipeline.RequestResolved(*a, ramDemand, produce);
  CHECK(admitted.has_value(), "the same previously refused demand now fits unchanged limits");
  if (!admitted) { return; }
  auto metadata = Wait(*admitted);
  CHECK(metadata.Status == AssetPublicationStatus::Ready && metadata.Receipt && !metadata.Native,
        "existing metadata-only producers retain their publication contract");
}

void WholePackageBudget() {
  StoreFixture fixture;
  CHECK(fixture.Open(), "the capacity counterexample opens its real package");
  if (!fixture.Cache) { return; }
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = kReservation,
                          .ReceiptBytesMost = 1024});
  auto consumer = pipeline.OpenConsumer({.Identity = fixture.Cache});
  CHECK(consumer.has_value(), "a consumer opens under the deliberately small output bound");
  if (!consumer) { return; }
  std::atomic<size_t> capacity{0};
  std::atomic<size_t> slice{0};
  auto ticket = pipeline.RequestLogical(
      *consumer,
      {.Address = kAddress,
       .Generation = consumer->Generation(),
       .InputBytes = 1024,
       .ReceiptBytesMost = 1024},
      [cache = fixture.Cache, &capacity, &slice](
          const std::stop_token &) -> std::expected<std::optional<AssetProduct>, std::string> {
        auto loaded = cache->LoadRequest(kAddress, kPackageBytes);
        if (!loaded || !*loaded) { return std::unexpected("bound native content did not load"); }
        AssetProduct product(std::move(**loaded));
        capacity.store(product.CapacityBytes());
        slice.store(product.Native()->Bytes().size());
        return product;
      });
  CHECK(ticket.has_value(), "the bounded lookup is admitted before it loads the native package");
  if (!ticket) { return; }
  const auto failed = Wait(*ticket);
  CHECK(slice.load() == 32 && capacity.load() >= kPackageBytes,
        "a small record slice actually retains its whole 8192-byte package");
  CHECK(failed.Status == AssetPublicationStatus::Failed &&
            failed.Error == AssetPublicationError::InvalidReceipt && !failed.Native &&
            !failed.Receipt,
        "the whole retained package must fit admitted input RAM before any publication escapes");
  ticket = std::unexpected(AssetPublicationError::InvalidRequest);
  CHECK(pipeline.Counts().Jobs == 0 && pipeline.Counts().InputBytes == 0 &&
            pipeline.Counts().ReceiptBytes == 0,
        "rejected native output releases its backing and reservation without partial results");
}
}

int main() {
  SharedNativeResult();
  WholePackageBudget();
  return Report();
}
