#include "Check.h"
#include "ContentStore.h"
#include "CopernicusDem.h"
#include "SourceSet.h"
#include "TilePool.h"

#include <array>
#include <memory>
#include <string>

namespace {
using namespace outshine::Data;

class Http final : public Transport {
public:
  int Begins = 0;

  FetchStart Begin(const std::string &) override { return Ticket::None; }

  FetchStart Begin(const std::string &, ByteRange range, std::string_view tag) override {
    ++Begins;
    Range = {.Bytes = range, .TotalBytes = 1000, .EntityTag = std::string(tag)};
    return Ticket{1};
  }

  Wire Collect(Ticket) override { return Wire::Answered({4, 5, 6}, Range); }

  void Cancel(Ticket) override {}

  RangeResponse Range;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<CopernicusDem>()) == SourceSet::Registration::Accepted,
        "native source uses normal provider dispatch");
  Http wire;
  TilePool pool({.ByteBudget = 1024, .Carriers = 1}, sources, wire);
  const Fetch request(DataKind::Elevation,
                      Address::AtCell({54, 9}),
                      ByteRange{.First = 100, .Length = 3},
                      "\"original-revision\"");
  TilePool::Landing live, warm;
  CHECK(
      pool.BytesBlocking(request, &live) == TilePool::Reply::Ready && live.Range &&
          live.Range->Bytes == *request.Range() && live.Range->EntityTag == request.EntityTag() &&
          live.Range->TotalBytes == 1000 && live.At == request.Where(),
      "runtime IO landing retains native source object, original range, revision and file length");
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
