#include "Check.h"
#include "NativeCog.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "TilePool.h"
#include "TileGeodesy.h"
#include "StructureSourceKey.h"

#include <atomic>
#include <algorithm>
#include <array>
#include <cstdio>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <memory>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Ground;

class Http final : public Transport {
public:
  std::atomic<int> Begins{0}, Pinned{0};
  bool Missing = false;

  FetchStart Begin(const std::string &) override {
    return std::unexpected(FetchFailureReason::InvalidRequest);
  }

  FetchStart Begin(const std::string &url, ByteRange range, std::string_view tag) override {
    const std::scoped_lock lock(Mutex);
    ++Begins;
    if (!tag.empty()) { ++Pinned; }
    const Ticket ticket{static_cast<uint64_t>(Begins.load())};
    Requests.emplace(ticket, std::pair{url, range});
    return ticket;
  }

  Wire Collect(Ticket ticket) override {
    const std::scoped_lock lock(Mutex);
    const auto found = Requests.find(ticket);
    if (found == Requests.end()) { return Wire::Working(); }
    const auto [url, range] = found->second;
    int south = 0, west = 0;
    CHECK(std::sscanf(url.c_str(), "g/%d/%d", &south, &west) == 2,
          "source IO uses native cell addresses");
    auto &bytes = Objects[url];
    if (bytes.empty()) { bytes = Test::NativeCog({.SouthDeg = south, .WestDeg = west}, Missing); }
    CHECK(range.First <= bytes.size() && range.Length <= bytes.size() - range.First,
          "source receives original byte intervals");
    if (range.First > bytes.size() || range.Length > bytes.size() - range.First) {
      return Wire::Answered(416, {});
    }
    Requests.erase(found);
    return Wire::Answered(
        std::vector<uint8_t>(bytes.begin() + static_cast<ptrdiff_t>(range.First),
                             bytes.begin() + static_cast<ptrdiff_t>(range.First + range.Length)),
        {.Bytes = range, .TotalBytes = bytes.size(), .EntityTag = "\"original-native\""});
  }

  void Cancel(Ticket ticket) override {
    const std::scoped_lock lock(Mutex);
    Requests.erase(ticket);
  }

private:
  std::mutex Mutex;
  std::map<Ticket, std::pair<std::string, ByteRange>> Requests;
  std::map<std::string, std::vector<uint8_t>> Objects;
};

class Native final : public Source {
public:
  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &request) const noexcept override {
    return request.Kind() == DataKind::Elevation && request.Where().Cell() && request.Range()
               ? Coverage::Inside
               : Coverage::Outside;
  }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override {
    return std::unexpected(FetchFailureReason::InvalidRequest);
  }

  FetchStart Begin(const Fetch &request, Transport &transport) const override {
    return transport.Begin(request.Where().Text(), *request.Range(), request.EntityTag());
  }

  Fetched Collect(const Address &, Ticket ticket, Transport &transport) const override {
    auto response = transport.Collect(ticket);
    auto body = response.Take();
    return body ? Fetched::Delivered(std::move(body->Body), std::move(body->Range))
                : Fetched::Working();
  }

private:
  SourceDecl Decl{.Id = "external-native",
                  .Revision = "dataset",
                  .Kind = DataKind::Elevation,
                  .How = Scheme::GeographicCell,
                  .Wire = WireFormat::CopernicusCog,
                  .MaximumPayloadBytes = 8u * 1024 * 1024};
};

TilePool::Reply Field(TilePool &pool,
                      TileId at,
                      std::shared_ptr<const TerrainField> &field,
                      std::optional<FetchFailure> &failure) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  auto state = TilePool::Reply::Pending;
  while (std::chrono::steady_clock::now() < end) {
    state = pool.Field(at, &field, &failure);
    if (state != TilePool::Reply::Pending && state != TilePool::Reply::Deferred) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return state;
}
}

int main() {
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<Native>()) == SourceSet::Registration::Accepted,
        "external native provider registers through the common interface");
  Http http;
  TilePool pool({.ByteBudget = 8u * 1024 * 1024, .DecodedBytes = 4u * 1024 * 1024, .Carriers = 4},
                sources,
                http);
  const TileId at = *TileIndex::Of({.LongitudeDeg = 9.98, .LatitudeDeg = 54.02}, 7).Tile();
  std::shared_ptr<const TerrainField> field;
  std::optional<FetchFailure> failure;
  CHECK(Field(pool, at, field, failure) == TilePool::Reply::Ready && field,
        "shared compute worker publishes reprojected native heights");
  if (field) {
    CHECK(field->Rows() == 129 && field->Cols() == 129 && !field->HasMissingBoundary(),
          "native fields include complete source-cell and render-tile boundaries");
    double largest = 0;
    for (uint32_t row = 0; row < field->Rows(); ++row) {
      for (uint32_t col = 0; col < field->Cols(); ++col) {
        const auto geo = TileFracToGeo({.X = at.X + static_cast<double>(col) / 128,
                                        .Y = at.Y + static_cast<double>(row) / 128},
                                       at.Zoom);
        largest = std::max(largest,
                           std::abs(static_cast<double>(field->AtM(row, col)) -
                                    NativeHeight(geo.LatitudeDeg, geo.LongitudeDeg)));
      }
    }
    CHECK(largest < 0.0001,
          "independent geographic plane survives variable longitude spacing, cell edges and "
          "Mercator reprojection");
    CHECK(field->Sources().size() > 1 &&
              std::ranges::all_of(field->Sources(),
                                  [](const auto &source) {
                                    return source.NativeCell &&
                                           source.Revision == "dataset:\"original-native\"";
                                  }),
          "all original native cells and pinned revisions remain provenance");
    CHECK(!field->Certificate().IsComplete(),
          "native samples alone do not invent a conservative runtime certificate");
    const int before = http.Begins;
    std::shared_ptr<const TerrainField> warm;
    CHECK(Field(pool, at, warm, failure) == TilePool::Reply::Ready && warm && http.Begins == before,
          "unchanged terrain is resident without source IO");
  }
  CHECK(http.Pinned > 0 && pool.Counters().FetchOnCompute == 0,
        "original block requests are revision pinned and IO never runs on compute");
  SourceSet absentSources(store);
  CHECK(absentSources.Add(std::make_unique<Native>()) == SourceSet::Registration::Accepted,
        "missing-post fixture registers");
  Http absentHttp;
  absentHttp.Missing = true;
  TilePool absent({.ByteBudget = 1024u * 1024, .Carriers = 1}, absentSources, absentHttp);
  std::shared_ptr<const TerrainField> none;
  CHECK(Field(absent, at, none, failure) == TilePool::Reply::Refused && !none && failure,
        "missing original heights refuse terrain instead of making zero ground");
  const std::optional<TileSourceIdentity> noVector;
  std::array<TileSourceIdentity, 1> first{
      {{.NativeCell = CellId{54, 9}, .SourceId = "native", .Revision = "original"}}};
  auto second = first;
  second[0].NativeCell->WestDeg = 10;
  CHECK(StructureSourceKey({.Vector = noVector, .HeightSources = first}) !=
            StructureSourceKey({.Vector = noVector, .HeightSources = second}),
        "structure terrain keys distinguish native source cells without fake Mercator addresses");
  return Report();
}
