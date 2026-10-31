#include "Check.h"
#include "OfflineSourceTransport.h"
#include "OsmField.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "VectorTileSource.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
constexpr auto kDataset = "openfreemap.openmaptiles";
constexpr auto kRevision = "pinned";
constexpr auto kEndpoint = "https://tiles.openfreemap.org/planet/pinned/{z}/{x}/{y}.pbf";
constexpr auto kSpecific = "x-ofm-debug: specific PBF planet pinned\r\n";
constexpr int kNotFound = 404;
constexpr int kTooMany = 429;
constexpr auto kAddress = TileId{.Zoom = 3, .X = 1, .Y = 2};

class Replies final : public SourceTransport {
public:
  std::string Headers;
  std::vector<uint8_t> Body{1, 2};
  int Status = 200;
  size_t Starts = 0;
  size_t BodyMost = std::numeric_limits<size_t>::max();

  FetchStart Begin([[maybe_unused]] const std::string &url) override {
    ++Starts;
    return static_cast<Ticket>(Starts);
  }

  FetchStart Begin(const std::string &url, size_t bodyBytesMost) override {
    BodyMost = bodyBytesMost;
    return Begin(url);
  }

  Wire Collect([[maybe_unused]] Ticket ticket) override {
    if (Body.size() > BodyMost) { return Wire::Unreachable(SourceFailureReason::CapacityRefused); }
    return Wire::Answered(Status, Body, 0.0, Headers);
  }

  void Cancel([[maybe_unused]] Ticket ticket) override {}
};

std::unique_ptr<Generators::Osm::VectorTileSource> MakeSource() {
  return std::make_unique<Generators::Osm::VectorTileSource>(
      kRevision,
      Rank{0},
      AbsencePolicy::Fail,
      kDataset,
      kEndpoint,
      Generators::Osm::MvtSchema::OpenMapTiles);
}

void CheckMissAndHit(const std::filesystem::path &root) {
  const auto at = Address::At(kAddress);
  const SourceRequest request(DataKind::VectorMap, at);
  {
    RawSourceCache cache({.Directory = root.string()});
    SourceSet sources(cache);
    auto source = MakeSource();
    const auto key = ContentKey(source->Declaration(), at);
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
          "the fixed-revision vector source is registered");
    Replies wire;
    constexpr std::array badHeaders{
        "",
        "x-ofm-debug: wildcard PBF planet\r\n",
        "x-ofm-debug: specific PBF planet other\r\n",
        "x-ofm-debug: specific PBF monaco pinned\r\n",
        "x-ofm-debug: empty tile wildcard\r\n",
        "x-ofm-debug: empty tile\r\n",
        "x-ofm-debug: specific PBF planet pinned\r\nx-ofm-debug: specific PBF planet pinned\r\n"};
    for (const auto *const headers : badHeaders) {
      wire.Headers = headers;
      auto query = sources.Ask(request);
      auto reply = sources.Collect(query, wire);
      CHECK(reply.Where() == SourceDelivery::State::Refused,
            "an HTTP200 response without unique matching identity is refused");
      CHECK(cache.Lookup(key).Where == RawSourceCache::Presence::Unknown,
            "wrong-release bytes neither poison the content cache nor establish absence");
    }
    wire.Headers.clear();
    wire.Status = kNotFound;
    auto missing = sources.Ask(request);
    CHECK(sources.Collect(missing, wire).Where() == SourceDelivery::State::Refused &&
              cache.Lookup(key).Where == RawSourceCache::Presence::Unknown,
          "an unqualified not-found cannot erase required coverage or persist as absence");
    wire.Status = 200;
    wire.Headers = kSpecific;
    auto query = sources.Ask(request);
    const auto good = sources.Collect(query, wire).Take();
    CHECK(good && good->Bytes == wire.Body && good->SourceId == kDataset,
          "the matching pinned response delivers the exact input bytes");
    CHECK(cache.Lookup(key).Bytes == wire.Body, "only the matching response enters the raw cache");
    CHECK(sources.Counters().HandedOver == 0, "no refusal is classified as authoritative absence");
  }
  RawSourceCache reopened({.Directory = root.string()});
  SourceSet warm(reopened);
  CHECK(warm.Add(MakeSource()) == SourceSet::Registration::Accepted,
        "a fresh process owner keeps the same declared source");
  OfflineSourceTransport offline;
  auto query = warm.Ask(request);
  const auto hit = warm.Collect(query, offline).Take();
  CHECK(hit && hit->Bytes == std::vector<uint8_t>({1, 2}) && warm.Counters().FromStore == 1 &&
            warm.Counters().ProviderStarts == 0,
        "a reopened offline hit adds no verification request or source mutation");
}

void CheckEmptyMissAndOfflineHit(const std::filesystem::path &root) {
  const auto at = Address::At(kAddress);
  const SourceRequest request(DataKind::VectorMap, at);
  {
    RawSourceCache cache({.Directory = root.string()});
    SourceSet sources(cache);
    auto source = MakeSource();
    const auto key = ContentKey(source->Declaration(), at);
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
          "the specifically empty source registers normally");
    Replies wire;
    wire.Body.clear();
    wire.Headers = "x-ofm-debug: empty tile\r\n";
    auto query = sources.Ask(request);
    const auto delivered = sources.Collect(query, wire).Take();
    CHECK(delivered && delivered->Bytes.empty() && delivered->SourceId == kDataset,
          "a qualified empty tile is delivered rather than retried or ranked as absent");
    const auto kept = cache.Lookup(key);
    CHECK(kept.Where == RawSourceCache::Presence::Bytes && kept.Bytes.empty(),
          "only the specifically confirmed empty payload enters the empty-input cache");
    CHECK(wire.Starts == 1 && sources.Counters().HandedOver == 0,
          "a valid empty tile starts one request and never falls back to another source");
  }
  RawSourceCache reopened({.Directory = root.string()});
  SourceSet warm(reopened);
  CHECK(warm.Add(MakeSource()) == SourceSet::Registration::Accepted,
        "a fresh owner reopens the exact empty source declaration");
  OfflineSourceTransport offline;
  auto query = warm.Ask(request);
  const auto hit = warm.Collect(query, offline).Take();
  CHECK(hit && hit->Bytes.empty() && warm.Counters().FromStore == 1 &&
            warm.Counters().ProviderStarts == 0,
        "a fresh offline process restores complete empty bytes without another HTTP request");
  if (!hit) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  Generators::Osm::OsmField field(kAddress.Zoom, layers, Generators::Osm::MvtSchema::OpenMapTiles);
  const auto accepted =
      field.Accept(static_cast<int>(kAddress.X), static_cast<int>(kAddress.Y), hit->Bytes);
  CHECK(accepted && field.Tiles().size() == 1 && field.Features().empty(),
        "the actual MVT consumer accepts the complete zero-feature tile");
}

void CheckRenamedProvider() {
  Replies wire;
  const auto at = Address::At(kAddress);
  const Generators::Osm::VectorTileSource renamed(
      kRevision,
      Rank{0},
      AbsencePolicy::Fail,
      "renamed.vector",
      "HTTPS://TILES.OPENFREEMAP.ORG:443/region/pinned/{z}/{x}/{y}.pbf?token=public");
  const auto missing = renamed.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(missing && missing->What == Meaning::Refused,
        "renaming the dataset cannot bypass the actual provider's release contract");
  wire.Headers = "x-ofm-debug: specific PBF region pinned\r\n";
  const auto matched = renamed.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(matched && matched->What == Meaning::Bytes,
        "the configured source area and revision qualify a renamed official endpoint");
  const Generators::Osm::VectorTileSource mismatched(
      kRevision,
      Rank{0},
      AbsencePolicy::Fail,
      kDataset,
      "https://tiles.openfreemap.org/planet/other/{z}/{x}/{y}.pbf");
  wire.Headers = kSpecific;
  const auto wrong = mismatched.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(wrong && wrong->What == Meaning::Refused,
        "a matching response header cannot repair a differently bound request URL");
  const Generators::Osm::VectorTileSource missingArea(
      kRevision,
      Rank{0},
      AbsencePolicy::Fail,
      kDataset,
      "https://tiles.openfreemap.org/pinned/{z}/{x}/{y}.pbf");
  wire.Headers = "x-ofm-debug: specific PBF tiles.openfreemap.org pinned\r\n";
  const auto absentArea = missingArea.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(absentArea && absentArea->What == Meaning::Refused,
        "the URL authority is never mistaken for a source area");
}

void CheckOtherOutcomes() {
  auto source = MakeSource();
  Replies wire;
  const auto at = Address::At(kAddress);
  wire.Headers = "x-ofm-debug: empty tile\r\n";
  wire.Body.clear();
  const auto empty = source->Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(empty && empty->What == Meaning::Bytes && empty->Bytes.empty(),
        "the specifically confirmed empty vector tile is a valid zero-feature input");
  wire.Headers.clear();
  wire.Status = kTooMany;
  const auto limited = source->Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(limited && limited->What == Meaning::Retry,
        "a temporary rate limit needs no fabricated source-identity header");
  wire.Status = 200;
  wire.Body = {1, 2};
  const Generators::Osm::VectorTileSource generic(kRevision,
                                                  Rank{0},
                                                  AbsencePolicy::Fail,
                                                  "fixture.vector",
                                                  "https://fixture.example/{z}/{x}/{y}.pbf");
  const auto other = generic.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(other && other->What == Meaning::Bytes && other->Bytes == wire.Body,
        "an independent provider does not inherit OpenFreeMap's response protocol");
  const Generators::Osm::VectorTileSource sameName(
      kRevision, Rank{0}, AbsencePolicy::Fail, kDataset, "https://fixture.example/{z}/{x}/{y}.pbf");
  const auto independent = sameName.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(independent && independent->What == Meaning::Bytes,
        "a dataset identifier alone cannot impose a different endpoint's HTTP protocol");
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::path(PlantedPath("pinned-vector-response"));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  CHECK(!error, "the isolated raw cache is available");
  CheckMissAndHit(root);
  CheckEmptyMissAndOfflineHit(root / "empty");
  CheckOtherOutcomes();
  CheckRenamedProvider();
  std::filesystem::remove_all(root, error);
  CHECK(!error, "only test-owned cache files are removed after closing their owners");
  return Report();
}
