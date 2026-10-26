#include "Check.h"
#include "RawSourceCache.h"
#include "CopernicusDemSource.h"
#include "SourceSet.h"
#include "TilePool.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace {
using namespace outshine::Data;

class Http final : public SourceTransport {
public:
  int Begins = 0;

  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  FetchStart Begin(const std::string & /*url*/, ByteRange range, std::string_view tag) override {
    ++Begins;
    Range = {.Bytes = range, .TotalBytes = 1000, .EntityTag = std::string(tag)};
    return Ticket{1};
  }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Answered({4, 5, 6}, Range); }

  void Cancel(Ticket /*ticket*/) override {}

  RangeResponse Range;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<outshine::Generators::Terrain::CopernicusDemSource>()) ==
            SourceSet::Registration::Accepted,
        "native source uses normal provider dispatch");
  Http wire;
  TilePool pool({.ByteBudget = 1024, .Carriers = 1, .PreparedFields = {}}, sources, wire);
  const ByteRange requestedRange{.First = 100, .Length = 3};
  const SourceRequest request(DataKind::Elevation,
                              Address::AtCell({.SouthDeg = 54, .WestDeg = 9}),
                              requestedRange,
                              "\"original-revision\"");
  TilePool::Landing live;
  TilePool::Landing warm;
  CHECK(
      pool.BytesBlocking(request, &live) == TilePool::Reply::Ready && live.Range &&
          live.Range->Bytes == requestedRange && live.Range->EntityTag == request.EntityTag() &&
          live.Range->TotalBytes == 1000 && live.At == request.Where(),
      "runtime IO landing retains native source object, original range, revision and file length");
  if (!live.Range) { return Report(); }
  CHECK(pool.BytesBlocking(request, &warm) == TilePool::Reply::Ready && warm.Range &&
            warm.Range->Bytes == live.Range->Bytes &&
            warm.Range->EntityTag == live.Range->EntityTag &&
            warm.Range->TotalBytes == live.Range->TotalBytes && warm.Bytes == live.Bytes &&
            wire.Begins == 1,
        "resident byte reuse cannot discard the receipt or start duplicate source IO");
  CHECK(pool.ByteCacheBytes() >=
            sizeof(RangeResponse) + request.EntityTag().size() + live.Bytes.size(),
        "runtime residency accounting includes receipt and its owned revision string");
  return Report();
}
