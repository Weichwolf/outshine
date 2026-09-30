#include "Check.h"
#include "SourceSet.h"
#include <world/data/Source.h>

#include <memory>
#include <string>

namespace {
using namespace outshine::Data;

class WireProbe final : public Transport {
public:
  int Cancelled = 0;

  FetchStart Begin(const std::string &) override { return Ticket{17}; }

  Wire Collect(Ticket) override { return Wire::Working(); }

  void Cancel(Ticket) override { ++Cancelled; }
};

class OwnedWork final : public Source {
public:
  mutable int Cancelled = 0;
  mutable Ticket Last = Ticket::None;

  OwnedWork() { Decl_.Id = "provider-owned-work"; }

  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket{23}; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Working();
  }

  void Cancel(Ticket ticket, Transport &) const override {
    ++Cancelled;
    Last = ticket;
  }

private:
  SourceDecl Decl_;
};
}

int main() {
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  auto source = std::make_unique<OwnedWork>();
  const auto *observed = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
  WireProbe wire;
  auto query = sources.Ask(Fetch(DataKind::Elevation, Address::At({.Zoom = 3, .X = 1, .Y = 2})));
  CHECK(sources.Collect(query, wire).Where() == Delivery::State::Pending,
        "provider owns pending work");
  SourceSet::Abandon(query, wire);
  CHECK(observed->Cancelled == 1 && observed->Last == Ticket{23} && wire.Cancelled == 0,
        "cancellation returns the exact ticket to the owning provider instead of a foreign "
        "transport");
  SourceSet::Abandon(query, wire);
  CHECK(observed->Cancelled == 1, "finished queries never cancel work twice");
  return Report();
}
