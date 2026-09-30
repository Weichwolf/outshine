#include "Check.h"

#include <world/data/Fetch.h>
#include <world/data/Fetched.h>
#include <world/data/Source.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class Legacy final : public Source {
public:
  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override {
    ++Begins;
    return Ticket::None;
  }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered({1});
  }

  mutable int Begins = 0;

private:
  SourceDecl Decl_;
};

class Unused final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};
}

int main() {
  using namespace outshine::Test;
  const auto at = Address::AtCell({54, 9});
  const ByteRange range{.First = 100, .Length = 3};
  std::string pin = "\"revision-1\"";
  const Fetch partial(DataKind::Elevation, at, range, pin);
  pin.clear();
  CHECK(partial.Where() == at && partial.Range() == range &&
            partial.EntityTag() == "\"revision-1\"",
        "native demand owns its interval and revision independently of caller storage");
  const Fetch whole(DataKind::Elevation, at);
  const Fetch unpinned(DataKind::Elevation, at, range);
  CHECK(!whole.Range() && whole.EntityTag().empty() && whole.Key() != partial.Key() &&
            partial.Key() != unpinned.Key(),
        "partial and whole demands and different revision pins have separate identities");
  CHECK((range.Valid() && !ByteRange{.First = 0, .Length = 0}.Valid() &&
         !ByteRange{.First = std::numeric_limits<uint64_t>::max(), .Length = 2}.Valid()),
        "empty and overflowing intervals are refused");
  CHECK(StrongEntityTag("\"\"") && StrongEntityTag("\"revision\"") &&
            !StrongEntityTag("W/\"revision\"") && !StrongEntityTag("\"bad\nvalue\"") &&
            !StrongEntityTag("\"bad\"value\"") &&
            !StrongEntityTag("\"" + std::string(MaximumEntityTagBytes, 'x') + "\""),
        "HTTP, provider and cache share the same strong-tag boundary");
  const RangeResponse origin{.Bytes = range, .TotalBytes = 1000, .EntityTag = "\"revision-1\""};
  CHECK((origin.Valid(3) && !origin.Valid(2) &&
         !RangeResponse{.Bytes = range, .TotalBytes = 102, .EntityTag = "\"revision-1\""}.Valid(3)),
        "receipt validates actual byte length and complete-file bounds");
  std::vector<uint8_t> bytes{4, 5, 6};
  const auto *original = bytes.data();
  auto received = Fetched::Delivered(std::move(bytes), origin);
  auto moved = std::move(received);
  auto assigned = Fetched::Working();
  assigned = std::move(moved);
  const auto reply = assigned.Take();
  CHECK(reply && reply->Bytes.data() == original &&
            reply->Bytes == std::vector<uint8_t>({4, 5, 6}) && reply->Range &&
            reply->Range->Bytes == range && reply->Range->TotalBytes == 1000 &&
            reply->Range->EntityTag == partial.EntityTag(),
        "receipt and original allocation survive both moves and a single ownership transfer");
  CHECK(received.Where() == Fetched::State::Consumed && moved.Where() == Fetched::State::Consumed &&
            !assigned.Take(),
        "moved and consumed replies cannot transfer source bytes twice");
  Legacy legacy;
  const Source &source = legacy;
  Unused transport;
  const auto refused = source.Begin(partial, transport);
  CHECK(!refused && refused.error() == FetchFailureReason::ProviderRefused && legacy.Begins == 0,
        "whole-payload providers cannot silently fetch an entire object for partial demand");
  CHECK(source.Begin(whole, transport) == Ticket::None && legacy.Begins == 1,
        "existing providers retain whole-payload acquisition through the shared entrypoint");
  return Report();
}
