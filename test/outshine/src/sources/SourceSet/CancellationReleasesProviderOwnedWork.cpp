#include "Check.h"
#include "SourceSet.h"
#include <Source.h>

#include <memory>
#include <string>
#include <utility>

namespace {
using namespace outshine::Data;
constexpr Ticket kTransportTicket{17};
constexpr Ticket kProviderTicket{23};

class WireProbe final : public Transport {
public:
  int Cancelled = 0;

  FetchStart Begin(const std::string & /*url*/) override { return kTransportTicket; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Working(); }

  void Cancel(Ticket /*ticket*/) override { ++Cancelled; }
};

struct OwnedWork final : Source {
  mutable int Cancelled = 0;
  mutable Ticket Last = Ticket::None;

  OwnedWork() { Decl_.Id = "provider-owned-work"; }

  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address & /*at*/, Transport & /*transport*/) const override {
    return kProviderTicket;
  }

  SourceResponse
  Collect(const Address & /*at*/, Ticket /*ticket*/, Transport & /*transport*/) const override {
    return SourceResponse::Working();
  }

  void Cancel(Ticket ticket, Transport & /*transport*/) const override {
    ++Cancelled;
    Last = ticket;
  }

private:
  SourceDecl Decl_;
};
}

int main() {
  using namespace outshine::Test;
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
  SourceSet sources(store);
  auto source = std::make_unique<OwnedWork>();
  const auto *observed = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
  WireProbe wire;
  auto query =
      sources.Ask(SourceRequest(DataKind::Elevation, Address::At({.Zoom = 3, .X = 1, .Y = 2})));
  CHECK(sources.Collect(query, wire).Where() == SourceDelivery::State::Pending,
        "provider owns pending work");
  SourceSet::Abandon(query, wire);
  CHECK(observed->Cancelled == 1 && observed->Last == kProviderTicket && wire.Cancelled == 0,
        "cancellation returns the exact ticket to the owning provider instead of a foreign "
        "transport");
  SourceSet::Abandon(query, wire);
  CHECK(observed->Cancelled == 1, "finished queries never cancel work twice");
  return Report();
}
