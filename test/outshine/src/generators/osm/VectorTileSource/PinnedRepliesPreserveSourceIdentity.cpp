#include "Check.h"
#include "OfflineSourceTransport.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "VectorTileSource.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
constexpr auto kDataset = "openfreemap.openmaptiles";
constexpr auto kRevision = "pinned";
constexpr auto kEndpoint = "https://example.test/planet/pinned/{z}/{x}/{y}.pbf";
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

  FetchStart Begin([[maybe_unused]] const std::string &url) override {
    ++Starts;
    return static_cast<Ticket>(Starts);
  }

  Wire Collect([[maybe_unused]] Ticket ticket) override {
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

void CheckOtherOutcomes() {
  auto source = MakeSource();
  Replies wire;
  const auto at = Address::At(kAddress);
  wire.Headers = "x-ofm-debug: empty tile\r\n";
  wire.Body.clear();
  const auto empty = source->Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(empty && empty->What == Meaning::Retry,
        "the specific empty route retains the existing empty-payload retry contract");
  wire.Headers.clear();
  wire.Status = kTooMany;
  const auto limited = source->Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(limited && limited->What == Meaning::Retry,
        "a temporary rate limit needs no fabricated source-identity header");
  wire.Status = 200;
  wire.Body = {1, 2};
  const Generators::Osm::VectorTileSource generic(
      kRevision, Rank{0}, AbsencePolicy::Fail, "fixture.vector", kEndpoint);
  const auto other = generic.Collect(at, static_cast<Ticket>(1), wire).Take();
  CHECK(other && other->What == Meaning::Bytes && other->Bytes == wire.Body,
        "an independent provider does not inherit OpenFreeMap's response protocol");
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::path(PlantedPath("pinned-vector-response"));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  CHECK(!error, "the isolated raw cache is available");
  CheckMissAndHit(root);
  CheckOtherOutcomes();
  std::filesystem::remove_all(root, error);
  CHECK(!error, "only test-owned cache files are removed after closing their owners");
  return Report();
}
