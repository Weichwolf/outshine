#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class NoTransport final : public Transport {
public:
  Ticket Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class ByteSource final : public Source {
public:
  SourceDecl Decl{.Id = "owned", .Keeps = Cacheability::Never};
  mutable std::vector<uint8_t> Bytes{1, 2, 3};
  mutable int Calls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    ++Calls;
    return Fetched::Delivered(std::move(Bytes));
  }
};
}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  auto source = std::make_unique<ByteSource>();
  const auto *probe = source.get();
  const auto *allocation = source->Bytes.data();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
  TilePool pool({}, sources, transport);
  const Fetch request(DataKind::Elevation, Address::Whole(0));
  TilePool::Landing first;
  CHECK(pool.BytesBlocking(request, &first) == TilePool::Reply::Ready,
        "source supplies tile bytes");
  CHECK(first.Bytes.data() == allocation && first.Bytes == std::vector<uint8_t>({1, 2, 3}),
        "caller receives the source allocation without an intermediate copy");
  if (!first.Bytes.empty()) { first.Bytes[0] = 9; }
  TilePool::Landing cached;
  CHECK(pool.BytesBlocking(request, &cached) == TilePool::Reply::Ready,
        "cached tile remains available");
  CHECK(cached.Bytes == std::vector<uint8_t>({1, 2, 3}) && probe->Calls == 1,
        "cache owns an independent snapshot and does not refetch");
  return Report();
}
