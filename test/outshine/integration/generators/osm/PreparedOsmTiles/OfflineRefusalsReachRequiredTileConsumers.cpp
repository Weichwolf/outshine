#include "Check.h"
#include "OsmCacheFixture.h"
#include "OfflineSourceTransport.h"
#include "OsmField.h"
#include "PreparedOsmTiles.h"
#include "SourceReader.h"
#include <array>
#include <atomic>
#include <chrono>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;

class MissingSource final : public Source {
public:
  explicit MissingSource(std::atomic_uint &calls,
                         Coverage coverage = Coverage::Inside,
                         Meaning response = Meaning::Refused)
      : Calls_(calls), Coverage_(coverage), Response_(response) {
    Decl_.Id = "native-offline-fixture";
    Decl_.Revision = "offline-r1";
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = 1024;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    return request.Kind() == Decl_.Kind ? Coverage_ : Coverage::Outside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*address*/, SourceTransport &transport) const override {
    ++Calls_;
    if (Response_ == Meaning::Absent) { return Ticket::None; }
    return transport.Begin("https://offline.invalid/vector");
  }

  SourceResponse Collect(const Address & /*address*/,
                         Ticket /*ticket*/,
                         SourceTransport & /*transport*/) const override {
    return SourceResponse::Meant(Response_);
  }

private:
  SourceDecl Decl_;
  std::atomic_uint &Calls_;
  Coverage Coverage_;
  Meaning Response_;
};

std::string AwaitFailure(OsmField &field, SourceReader &reader) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    const auto result = field.PollTile(reader, {.X = 1, .Y = 1});
    if (!result) { return result.error(); }
    if (*result) { return {}; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

void CheckAbsence(const std::filesystem::path &directory) {
  struct Input {
    Coverage Where;
    Meaning Response;
  };

  constexpr std::array inputs{Input{.Where = Coverage::Outside, .Response = Meaning::Refused},
                              Input{.Where = Coverage::Inside, .Response = Meaning::Absent}};
  for (const auto input : inputs) {
    std::atomic_uint calls{0};
    Tasks compute(1);
    OfflineSourceTransport transport;
    auto session =
        OsmCache::OpenSession(std::make_unique<MissingSource>(calls, input.Where, input.Response));
    CHECK(session, "absence uses the actual provider declaration and coverage");
    if (!session) { return; }
    auto prepared = PreparedOsmTiles::Open(directory.string(), *session, transport, compute);
    CHECK(prepared, "the absence consumer opens its isolated native source scope");
    if (!prepared) { return; }
    const std::array<std::string, 1> layers{"buildings"};
    const OsmField field(2, layers, MvtSchema::OpenMapTiles, *prepared);
    bool absent = false;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!absent && std::chrono::steady_clock::now() < until) {
      const auto delivery = (*prepared)->Acquire({.X = 1, .Y = 1}, field);
      CHECK(delivery, delivery ? "native absence polling remains valid" : delivery.error().c_str());
      if (!delivery) { break; }
      if (delivery->Status == PreparedOsmTiles::State::Pending) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      CHECK(delivery->Status == PreparedOsmTiles::State::Absent && !delivery->Failure &&
                !delivery->Product,
            "confirmed absence and undeclared input remain distinct from source refusal");
      absent = delivery->Status == PreparedOsmTiles::State::Absent;
      break;
    }
    CHECK(absent && calls == (input.Where == Coverage::Inside ? 1U : 0U) &&
              (*prepared)->Costs().Writes == 0,
          "confirmed and undeclared absence settle without network IO or fabricated assets");
  }
}
}

int main() {
  std::atomic_uint calls{0};
  std::shared_ptr<const SourceFailure> retained;
  std::weak_ptr<SourceSession> sourceLifetime;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-offline-refusal-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Tasks compute(1);
    OfflineSourceTransport transport;
    auto session = OsmCache::OpenSession(std::make_unique<MissingSource>(calls));
    CHECK(session, "the actual source session owns an offline-requesting provider");
    if (!session) { return Report(); }
    sourceLifetime = *session;
    auto prepared = PreparedOsmTiles::Open(directory.string(), *session, transport, compute);
    CHECK(prepared, "the native intermediate cache opens independently of source availability");
    if (!prepared) { return Report(); }
    const std::array<std::string, 1> layers{"buildings"};
    OsmField first(2, layers, MvtSchema::OpenMapTiles, *prepared);
    OsmField second(2, layers, MvtSchema::OpenMapTiles, *prepared);
    OsmCache::UnusedReader reader;
    const auto a = AwaitFailure(first, reader);
    const auto b = AwaitFailure(second, reader);
    for (const auto &error : {a, b}) {
      CHECK(error.contains("offline cache miss") && error.contains("native-offline-fixture") &&
                error.contains("offline-r1") && error.contains("vector/2/1/1"),
            "mandatory native consumers receive the source reason, identity and original address");
    }
    const auto left = (*prepared)->Acquire({.X = 1, .Y = 1}, first);
    const auto right = (*prepared)->Acquire({.X = 1, .Y = 1}, second);
    CHECK(left && right && left->Status == PreparedOsmTiles::State::Refused && left->Failure &&
              left->Failure == right->Failure && left->RetryAfterMs > 0,
          "coalesced native refusals share typed metadata and their retry hint");
    if (left && left->Failure) {
      retained = left->Failure;
      const auto tile = retained->Requested.Tile();
      const auto served = retained->Served ? retained->Served->Tile() : std::nullopt;
      CHECK(retained->Kind == DataKind::VectorMap &&
                retained->Reason == SourceFailureReason::OfflineMiss && tile &&
                (*tile == TileId{.Zoom = 2, .X = 1, .Y = 1}) && served && *served == *tile &&
                retained->SourceId == "native-offline-fixture" &&
                retained->SourceRevision == "offline-r1" && retained->SourceKey.size() == 64 &&
                !retained->HttpStatus && retained->Retries == 0,
            "the complete source failure retains kind, addresses, provenance, reason and retry "
            "count");
    }
    OsmField window(2, layers, MvtSchema::OpenMapTiles, *prepared);
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (window.RefusedTiles() == 0 && std::chrono::steady_clock::now() < until) {
      const auto built = window.Build(reader, {.LongitudeDeg = -45, .LatitudeDeg = 40}, 0, 1);
      CHECK(built, "the window retains a terminal source outcome without publishing a fake tile");
      if (!built) { break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(window.RefusedTiles() == 1 && window.SourceFailures().size() == 1 &&
              window.SourceFailures().front() == retained,
          "the current required window exposes its original typed source failure");
    const auto snapshot = window.SnapshotQueries();
    CHECK(snapshot && snapshot->SourceFailures().size() == 1 &&
              snapshot->SourceFailures().front() == retained,
          "an immutable query snapshot retains the same error receipt");
    CHECK((*prepared)->AdmissionCounts().InputBytes == 0 &&
              (*prepared)->AdmissionCounts().ReceiptBytes == 4096,
          "one shared failure receipt stays admitted without retaining input bytes");
    CHECK(calls == 1 && reader.Calls == 0 && (*prepared)->Costs().Writes == 0,
          "coalesced refusal uses one real source request without a reader bypass or fabricated "
          "asset");
  }
  CHECK(sourceLifetime.expired() && retained && retained->SourceId == "native-offline-fixture",
        "the typed failure survives all source and producer owners without retaining them");
  retained.reset();
  CheckAbsence(directory / "absent");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated refusal assets are removed");
  return Report();
}
