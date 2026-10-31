#include "AssetPipeline.h"
#include "AssetCache.h"
#include "Tasks.h"
#include "OsmField.h"
#include "MvtSchema.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr size_t kInput = size_t{1024} * 1024;
const std::string kAddress(64, 'b');

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-engine-owner-decode-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::shared_ptr<AssetCache> Cache;

  bool Open() {
    const std::array<std::string, 1> layers{"buildings"};
    OsmField field(2, layers, MvtSchema::OpenMapTiles);
    auto accepted = field.Accept(1, 1, Mvt::BuildingTile());
    if (!accepted) { return false; }
    auto bytes = field.EncodeNativeTile(kInput);
    if (!bytes) { return false; }
    std::error_code error;
    std::filesystem::create_directories(Directory, error);
    if (error) { return false; }
    auto cache = AssetCache::Open((Directory / "assets.sqlite").string());
    if (!cache) { return false; }
    Cache = std::move(*cache);
    const std::array records{AssetRecord{.Key = std::string(64, 'a'),
                                         .Kind = "osm-tile",
                                         .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                         .Package = {},
                                         .ByteCount = bytes->size(),
                                         .Parent = {},
                                         .RequestKey = kAddress}};
    return Cache->Publish(records, *bytes).has_value();
  }

  ~Fixture() {
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

struct DecodeTickets {
  AssetPipeline::RequestTicket First;
  AssetPipeline::RequestTicket Second;
};

void DecodeOwners(AssetPipeline &pipeline,
                  AssetPipeline::Consumer &consumer,
                  DecodeTickets tickets) {
  auto &first = tickets.First;
  auto &second = tickets.Second;
  auto publication = Wait(first);
  const auto other = Wait(second);
  CHECK(publication.Native && publication.Native == other.Native,
        "parallel consumers retain the same actual native OSM backing");
  if (!publication.Native) { return; }
  const size_t nativeBytes = pipeline.Counts().InputBytes;
  const auto bound = OsmField::NativeTileDecodeBytes(publication.Native->Bytes());
  CHECK(bound && *bound > 0, "native structure determines owner storage without allocation");
  if (!bound) { return; }
  CHECK(nativeBytes >= publication.Native->Bytes().size(),
        "the single complete package remains charged after publication");
  CHECK(first.SetConsumerBytes(*bound), "owner storage is admitted before native decode");
  auto decoded = OsmField::DecodeNativeTile(publication.Native->Bytes(), *bound);
  CHECK(decoded && decoded->Tiles().size() == 1 && decoded->HeapBytes() <= *bound,
        "actual normalized OSM data decodes within the admitted structural bound");
  CHECK(pipeline.Counts().InputBytes == nativeBytes + *bound,
        "native backing and decoded owner storage share the same RAM budget");
  const size_t remaining = 2 * kInput - pipeline.Counts().InputBytes;
  const auto deferred = second.SetConsumerBytes(remaining + 1);
  CHECK(!deferred && deferred.error() == AssetPublicationError::ResourceDeferred &&
            pipeline.Counts().InputBytes == nativeBytes + *bound,
        "a competing owner defers before allocation without changing retained accounting");
  const auto impossible = second.SetConsumerBytes(2 * kInput + 1);
  CHECK(!impossible && impossible.error() == AssetPublicationError::InvalidRequest,
        "an impossible owner bound is distinguished from temporary contention");
  first.Cancel();
  CHECK(pipeline.Counts().InputBytes == nativeBytes + *bound,
        "cancellation retains bytes while decoded data still belongs to the ticket");
  const auto cancelled = first.SetConsumerBytes(0);
  CHECK(!cancelled && cancelled.error() == AssetPublicationError::Cancelled,
        "cancelled demand cannot silently release live owner storage");
  decoded.reset();
  std::optional<AssetPipeline::RequestTicket> released(std::move(first));
  CHECK(pipeline.Counts().InputBytes == nativeBytes + *bound,
        "moving the owner ticket retains its reservation until the new owner releases it");
  released.reset();
  CHECK(pipeline.Counts().InputBytes == nativeBytes,
        "destroying the released owner ticket returns its decode reservation exactly once");
  CHECK(second.SetConsumerBytes(*bound), "another owner can proceed after storage is released");
  CHECK(second.SetConsumerBytes(*bound / 2) &&
            pipeline.Counts().InputBytes == nativeBytes + *bound / 2,
        "a smaller owner bound returns only that owner's unused storage");
  CHECK(consumer.Advance({2}), "the next owner generation invalidates existing demand");
  CHECK(pipeline.Counts().InputBytes == nativeBytes + *bound / 2,
        "a generation change retains storage until the old ticket is destroyed");
  const auto stale = second.SetConsumerBytes(*bound);
  CHECK(!stale && stale.error() == AssetPublicationError::Cancelled,
        "an invalidated owner cannot grow or reuse the next generation's budget");
}

void RetainAfterPipelineFacade(Fixture &fixture) {
  Tasks tasks(1);
  auto pipeline =
      std::make_unique<AssetPipeline>(tasks,
                                      AssetPipeline::Limits{.JobsMost = 1,
                                                            .ConsumersMost = 1,
                                                            .TicketsMost = 1,
                                                            .InputBytesMost = 2 * kInput,
                                                            .ReceiptBytesMost = 2048});
  auto consumer = pipeline->OpenConsumer({.Identity = fixture.Cache});
  CHECK(consumer, "a consumer owns its admitted state independently of the facade");
  if (!consumer) { return; }
  auto ticket = pipeline->RequestLogical(
      *consumer,
      {.Address = kAddress,
       .Generation = consumer->Generation(),
       .InputBytes = kInput,
       .ReceiptBytesMost = 2048},
      [cache = fixture.Cache](
          const std::stop_token &) -> std::expected<std::optional<AssetProduct>, std::string> {
        auto loaded = cache->LoadRequest(kAddress, kInput);
        if (!loaded || !*loaded) { return std::unexpected("native fixture lookup failed"); }
        return AssetProduct(std::move(**loaded));
      });
  CHECK(ticket, "the independent facade submits an actual native hit");
  if (!ticket) { return; }
  const auto publication = Wait(*ticket);
  CHECK(publication.Native && ticket->SetConsumerBytes(128),
        "the completed native ticket holds its planned decoder bytes");
  if (!publication.Native) { return; }
  pipeline.reset();
  const auto retained = AssetPipeline::RetainConsumerBytes(*consumer, *ticket);
  CHECK(retained && !publication.Native->Bytes().empty(),
        "a matching consumer retains actual native work after its pipeline facade is destroyed");
}
}

int main() {
  Fixture fixture;
  CHECK(fixture.Open(), "the actual cache stores one real normalized MVT tile");
  if (!fixture.Cache) { return Report(); }
  Tasks tasks(1);
  AssetPipeline pipeline(tasks,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 2,
                          .InputBytesMost = 2 * kInput,
                          .ReceiptBytesMost = 2048});
  auto consumer = pipeline.OpenConsumer({.Identity = fixture.Cache});
  CHECK(consumer, "a bounded native consumer opens in the cache's immutable scope");
  if (!consumer) { return Report(); }
  std::atomic_size_t reads{0};
  const auto produce =
      [cache = fixture.Cache,
       &reads](const std::stop_token &) -> std::expected<std::optional<AssetProduct>, std::string> {
    ++reads;
    auto loaded = cache->LoadRequest(kAddress, kInput);
    if (!loaded || !*loaded) { return std::unexpected("native fixture lookup failed"); }
    return AssetProduct(std::move(**loaded));
  };
  const AssetPipeline::LogicalRequest request{.Address = kAddress,
                                              .Generation = consumer->Generation(),
                                              .InputBytes = kInput,
                                              .ReceiptBytesMost = 2048};
  auto first = pipeline.RequestLogical(*consumer, request, produce);
  auto second = pipeline.RequestLogical(*consumer, request, produce);
  CHECK(first && second, "two early addresses share one admitted cache lookup");
  if (!first || !second) { return Report(); }
  AssetPipeline other(tasks,
                      {.JobsMost = 1,
                       .ConsumersMost = 1,
                       .TicketsMost = 1,
                       .InputBytesMost = kInput,
                       .ReceiptBytesMost = 2048});
  auto unrelated = other.OpenConsumer({.Identity = fixture.Cache});
  CHECK(unrelated, "another pipeline can hold the same store scope");
  if (!unrelated) { return Report(); }
  const auto crossed = AssetPipeline::RetainConsumerBytes(*unrelated, *first);
  CHECK(!crossed && crossed.error() == AssetPublicationError::InvalidRequest,
        "owner retention rejects a consumer from another pipeline even with the same scope");
  DecodeOwners(pipeline, *consumer, {.First = std::move(*first), .Second = std::move(*second)});
  CHECK(reads.load() == 1, "owner admission never regenerates or reloads shared native bytes");
  CHECK(pipeline.Counts().InputBytes == 0 && pipeline.Counts().Jobs == 0,
        "all owner reservations and native aliases retire after their last use");
  RetainAfterPipelineFacade(fixture);
  return Report();
}
