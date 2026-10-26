#include "Check.h"
#include <Source.h>
#include <SourceRequest.h>
#include <SourceResponse.h>
#include <SourceTransport.h>
#include <string>

namespace {
using namespace outshine::Data;

class WholePayloadSource final : public Source {
public:
  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers([[maybe_unused]] const SourceRequest &request) const noexcept override {
    return Coverage::Inside;
  }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin([[maybe_unused]] const Address &at,
                   [[maybe_unused]] SourceTransport &transport) const override {
    ++Begins_;
    return Ticket::None;
  }

  SourceResponse Collect([[maybe_unused]] const Address &at,
                         [[maybe_unused]] Ticket ticket,
                         [[maybe_unused]] SourceTransport &transport) const override {
    return SourceResponse::Delivered({1});
  }

  [[nodiscard]] int Begins() const noexcept { return Begins_; }

private:
  mutable int Begins_ = 0;
  SourceDecl Decl_;
};

class UnusedTransport final : public SourceTransport {
public:
  FetchStart Begin([[maybe_unused]] const std::string &url) override { return Ticket::None; }

  Wire Collect([[maybe_unused]] Ticket ticket) override { return Wire::Never(); }

  void Cancel([[maybe_unused]] Ticket ticket) override {}
};
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  constexpr int kSouthDeg = 54;
  constexpr int kWestDeg = 9;
  const auto at = Address::AtCell({.SouthDeg = kSouthDeg, .WestDeg = kWestDeg});
  constexpr ByteRange range{.First = 100, .Length = 3};
  const SourceRequest partial(DataKind::Elevation, at, range, "\"revision-1\"");
  const SourceRequest whole(DataKind::Elevation, at);
  const WholePayloadSource legacy;
  const Source &source = legacy;
  UnusedTransport transport;
  const auto refused = source.Begin(partial, transport);
  CHECK(!refused && refused.error() == SourceFailureReason::ProviderRefused && legacy.Begins() == 0,
        "whole-payload providers cannot silently fetch an entire object for partial demand");
  CHECK(source.Begin(whole, transport) == Ticket::None && legacy.Begins() == 1,
        "existing providers retain whole-payload acquisition through the shared entrypoint");
  return Report();
}
