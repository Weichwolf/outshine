#include "Check.h"
#include "NativeCog.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "TilePool.h"
#include "TileGeodesy.h"
#include "StructureSourceKey.h"

#include <atomic>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <memory>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Ground;

class Http final : public Transport {
public:
  explicit Http(bool missing = false) : Missing_(missing) {}

  [[nodiscard]] int Begins() const noexcept { return Begins_.load(); }

  [[nodiscard]] int Pinned() const noexcept { return Pinned_.load(); }

  FetchStart Begin(const std::string & /*url*/) override {
    return std::unexpected(SourceFailureReason::InvalidRequest);
  }

  FetchStart Begin(const std::string &url, ByteRange range, std::string_view tag) override {
    const std::scoped_lock lock(Mutex);
    ++Begins_;
    if (!tag.empty()) { ++Pinned_; }
    const Ticket ticket{static_cast<uint64_t>(Begins_.load())};
    Requests.emplace(ticket, std::pair{url, range});
    return ticket;
  }

  Wire Collect(Ticket ticket) override {
    const std::scoped_lock lock(Mutex);
    const auto found = Requests.find(ticket);
    if (found == Requests.end()) { return Wire::Working(); }
    const auto [url, range] = found->second;
    int south = 0;
    int west = 0;
    CHECK(std::sscanf(url.c_str(), "g/%d/%d", &south, &west) == 2,
          "source IO uses native cell addresses");
    auto &bytes = Objects[url];
    if (bytes.empty()) { bytes = Test::NativeCog({.SouthDeg = south, .WestDeg = west}, Missing_); }
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
  std::atomic<int> Begins_{0}, Pinned_{0};
  const bool Missing_;
  std::mutex Mutex;
  std::map<Ticket, std::pair<std::string, ByteRange>> Requests;
  std::map<std::string, std::vector<uint8_t>> Objects;
};

class Native final : public Source {
public:
  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl; }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    return request.Kind() == DataKind::Elevation && request.Where().Cell() && request.Range()
               ? Coverage::Inside
               : Coverage::Outside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*at*/, Transport & /*transport*/) const override {
    return std::unexpected(SourceFailureReason::InvalidRequest);
  }

  FetchStart Begin(const SourceRequest &request, Transport &transport) const override {
    const auto range = request.Range();
    if (!range) { return std::unexpected(SourceFailureReason::InvalidRequest); }
    return transport.Begin(request.Where().Text(), *range, request.EntityTag());
  }

  SourceResponse
  Collect(const Address & /*at*/, Ticket ticket, Transport &transport) const override {
    auto response = transport.Collect(ticket);
    auto body = response.Take();
    return body ? SourceResponse::Delivered(std::move(body->Body), std::move(body->Range))
                : SourceResponse::Working();
  }

private:
  SourceDecl Decl{.Id = "external-native",
                  .Revision = "dataset",
                  .Endpoint = {},
                  .Kind = DataKind::Elevation,
                  .How = Scheme::GeographicCell,
                  .Wire = WireFormat::CopernicusCog,
                  .MaximumPayloadBytes = 8u * 1024 * 1024,
                  .PayloadSha256 = {},
                  .Schema = {}};
};

TilePool::Reply Field(TilePool &pool,
                      TileId at,
                      std::shared_ptr<const TerrainField> &field,
                      std::optional<SourceFailure> &failure) {
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
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<Native>()) == SourceSet::Registration::Accepted,
        "external native provider registers through the common interface");
  Http http;
  TilePool pool({.ByteBudget = 8u * 1024 * 1024,
                 .DecodedBytes = 4u * 1024 * 1024,
                 .Carriers = 4,
                 .PreparedFields = {}},
                sources,
                http);
  const auto requested = TileIndex::Of({.LongitudeDeg = 9.98, .LatitudeDeg = 54.02}, 7).Tile();
  CHECK(requested.has_value(), "fixture camera has a native geographic tile");
  if (!requested) { return Report(); }
  const TileId at = *requested;
  std::shared_ptr<const TerrainField> field;
  std::optional<SourceFailure> failure;
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
    const int before = http.Begins();
    std::shared_ptr<const TerrainField> warm;
    CHECK(Field(pool, at, warm, failure) == TilePool::Reply::Ready && warm &&
              http.Begins() == before,
          "unchanged terrain is resident without source IO");
  }
  CHECK(http.Pinned() > 0 && pool.Counters().FetchOnCompute == 0,
        "original block requests are revision pinned and IO never runs on compute");
  SourceSet absentSources(store);
  CHECK(absentSources.Add(std::make_unique<Native>()) == SourceSet::Registration::Accepted,
        "missing-post fixture registers");
  Http absentHttp(true);
  TilePool absent(
      {.ByteBudget = 1024u * 1024, .Carriers = 1, .PreparedFields = {}}, absentSources, absentHttp);
  std::shared_ptr<const TerrainField> none;
  CHECK(Field(absent, at, none, failure) == TilePool::Reply::Refused && !none && failure,
        "missing original heights refuse terrain instead of making zero ground");
  const std::optional<TileSourceIdentity> noVector;
  std::array<TileSourceIdentity, 1> first{{{.NativeCell = CellId{.SouthDeg = 54, .WestDeg = 9},
                                            .SourceId = "native",
                                            .Revision = "original"}}};
  auto second = first;
  second[0].NativeCell = CellId{.SouthDeg = 54, .WestDeg = 10};
  CHECK(StructureSourceKey({.Vector = noVector, .HeightSources = first}) !=
            StructureSourceKey({.Vector = noVector, .HeightSources = second}),
        "structure terrain keys distinguish native source cells without fake Mercator addresses");
  return Report();
}
