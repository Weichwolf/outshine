#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"
#include <chrono>
#include <cstdint>
#include <thread>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class NoTransport final : public SourceTransport {
public:
  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Never(); }

  void Cancel(Ticket /*ticket*/) override {}
};

class ByteSource final : public Source {
public:
  SourceDecl Decl{.Id = "owned",
                  .Revision = "fixture-r1",
                  .Endpoint = {},
                  .Keeps = Cacheability::Never,
                  .PayloadSha256 = {},
                  .Schema = {}};
  mutable std::vector<uint8_t> Bytes{1, 2, 3};
  mutable int Calls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address & /*at*/, SourceTransport & /*transport*/) const override {
    return Ticket::None;
  }

  SourceResponse Collect(const Address & /*at*/,
                         Ticket /*ticket*/,
                         SourceTransport & /*transport*/) const override {
    ++Calls;
    return SourceResponse::Delivered(std::move(Bytes));
  }
};
}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  auto source = std::make_unique<ByteSource>();
  const auto *probe = source.get();
  const auto *allocation = source->Bytes.data();
  const auto sourceKey = SourceKey(source->Decl);
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
  TilePool pool({}, sources, transport);
  const SourceRequest request(DataKind::Elevation, Address::Whole(0));
  TilePool::Landing first;
  CHECK(pool.BytesBlocking(request, &first) == TilePool::Reply::Ready,
        "source supplies tile bytes");
  CHECK(first.Bytes.data() == allocation && first.Bytes == std::vector<uint8_t>({1, 2, 3}),
        "caller receives the source allocation without an intermediate copy");
  CHECK(first.SourceId == "owned" && first.SourceRevision == "fixture-r1" &&
            first.SourceKey == sourceKey,
        "caller receives the declared source identity with tile bytes");
  CHECK(sources.Add(std::make_unique<ByteSource>()) == SourceSet::Registration::Sealed &&
            sources.Count() == 1,
        "tile pool seals its source registry before carrier work starts");
  if (!first.Bytes.empty()) { first.Bytes[0] = 9; }
  TilePool::Landing cached;
  CHECK(pool.BytesBlocking(request, &cached) == TilePool::Reply::Ready,
        "cached tile remains available");
  CHECK(cached.Bytes == std::vector<uint8_t>({1, 2, 3}) && cached.SourceId == "owned" &&
            cached.SourceRevision == "fixture-r1" && cached.SourceKey == sourceKey &&
            probe->Calls == 1,
        "cache owns an independent payload and provenance snapshot without refetching");
  SourceReader &reader = pool;
  auto delivery = reader.Poll(request);
  auto answer = delivery.Take();
  CHECK(answer.has_value(), "generic source reads reuse the resident delivery");
  if (!answer) { return Report(); }
  CHECK(answer->Bytes == cached.Bytes && answer->SourceId == cached.SourceId &&
            answer->SourceRevision == cached.SourceRevision && answer->SourceKey == sourceKey &&
            answer->At == request.Where() && probe->Calls == 1,
        "the generic adapter preserves bytes, served address and provenance without refetching");
  CHECK(pool.Counters().MeshAttempts == 0 && pool.Counters().FieldAttempts == 0,
        "generic source reads do not generate terrain fields or meshes");
  CHECK(reader.Poll(SourceRequest(DataKind::Elevation, Address::Whole(1))).Where() ==
            SourceDelivery::State::Pending,
        "a fresh request starts a carrier job");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (pool.Counters().Held == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  CHECK(pool.Counters().Held == 1, "the carrier result exists before the landing wait");
  const auto waiting = std::chrono::steady_clock::now();
  CHECK(pool.AwaitLanding(0.25), "a pre-landed result is observable without another notification");
  CHECK(std::chrono::steady_clock::now() - waiting < std::chrono::milliseconds(200),
        "a pre-landed result avoids waiting for the deadline");
  CHECK(pool.Counters().Outstanding == 0,
        "a completed result retained for its caller is not counted as outstanding work");
  CHECK(pool.ResidentBytes() == pool.ByteCacheBytes() + pool.DemCacheBytes() +
                                    pool.SchedulerBytes() + pool.TerrainMetadataBytes(),
        "resident tile memory includes transport, terrain, revision metadata and scheduler storage "
        "exactly once");
  return Report();
}
