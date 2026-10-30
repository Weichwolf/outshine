#include "SourceSession.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;
using namespace outshine::Test;

struct Calls {
  unsigned Began = 0;
  unsigned Canceled = 0;
  unsigned Destroyed = 0;
  bool Ready = false;
  Ticket Last = Ticket::None;
};

class WireProbe final : public SourceTransport {
public:
  unsigned Began = 0;
  unsigned Canceled = 0;

  FetchStart Begin(const std::string & /*url*/) override {
    ++Began;
    return Ticket{99};
  }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Working(); }

  void Cancel(Ticket /*ticket*/) override { ++Canceled; }
};

class OwnedSource final : public Source {
public:
  OwnedSource(std::shared_ptr<Calls> calls, std::string revision) : Calls_(std::move(calls)) {
    Decl_.Id = "session-read-fixture";
    Decl_.Revision = std::move(revision);
  }

  ~OwnedSource() override { ++Calls_->Destroyed; }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin(const Address & /*at*/,
                                 SourceTransport & /*transport*/) const override {
    return Ticket{static_cast<uint64_t>(++Calls_->Began)};
  }

  [[nodiscard]] SourceResponse Collect(const Address & /*at*/,
                                       Ticket /*ticket*/,
                                       SourceTransport & /*transport*/) const override {
    return Calls_->Ready ? SourceResponse::Delivered({3, 5, 8}) : SourceResponse::Working();
  }

  void Cancel(Ticket ticket, SourceTransport & /*transport*/) const override {
    ++Calls_->Canceled;
    Calls_->Last = ticket;
  }

private:
  std::shared_ptr<Calls> Calls_;
  SourceDecl Decl_;
};

class OwnedProvider final : public Provider {
public:
  explicit OwnedProvider(std::shared_ptr<Calls> calls) : Calls_(std::move(calls)) {}

  [[nodiscard]] std::string_view kind() const override { return "session-read-fixture"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &declaration,
       std::string_view /*builtinDataDirectory*/) const override {
    return std::make_unique<OwnedSource>(Calls_, declaration.Revision);
  }

private:
  std::shared_ptr<Calls> Calls_;
};

struct Fixture {
  std::shared_ptr<Calls> State = std::make_shared<Calls>();
  OwnedProvider Provider{State};
  ProviderRegistry Registry;
  WireProbe Wire;

  [[nodiscard]] std::expected<std::shared_ptr<SourceSession>, std::string> Open() {
    if (!Registry.registerProvider(Provider)) {
      return std::unexpected("fixture registration failed");
    }
    const std::array declarations{SourceProvider{.Kind = "session-read-fixture",
                                                 .Revision = "r1",
                                                 .Dataset = {},
                                                 .Location = {},
                                                 .Endpoint = {},
                                                 .Coverage = std::nullopt,
                                                 .PayloadSha256 = {},
                                                 .Schema = {}}};
    return SourceSession::Open(
        {.Directory = {}, .Using = RawSourceCache::Use::Off}, declarations, {}, Registry);
  }
};

SourceRequest Request(uint32_t x = 1) {
  return SourceRequest(DataKind::Elevation, Address::At({.Zoom = 3, .X = x, .Y = 2}));
}

void OwnershipAndMove() {
  Fixture fixture;
  auto session = fixture.Open();
  CHECK(session.has_value(), "the provider-backed source session opens");
  if (!session) { return; }
  const std::weak_ptr<SourceSession> retained = *session;
  auto read = SourceSession::Request(*session, Request(), fixture.Wire);
  CHECK(read.has_value() && fixture.State->Began == 0 && fixture.Wire.Began == 0,
        "source selection starts no provider or transport IO");
  if (!read) { return; }
  session->reset();
  CHECK(!retained.expired() && fixture.State->Destroyed == 0,
        "the input request keeps its configured providers and source cache alive");
  CHECK(read->Poll().Where() == SourceDelivery::State::Pending && fixture.State->Began == 1,
        "the first worker poll starts one provider-owned request");
  auto moved = std::move(*read);
  CHECK(read->Poll().Where() == SourceDelivery::State::Consumed && fixture.State->Canceled == 0,
        "the moved-from request cannot poll or cancel the transferred provider ticket");
  CHECK(moved.Poll().Where() == SourceDelivery::State::Pending && fixture.State->Began == 1,
        "the moved request resumes its original work without restarting input IO");
  moved.Cancel();
  CHECK(fixture.State->Canceled == 1 && fixture.State->Last == Ticket{1} &&
            fixture.Wire.Canceled == 0 && retained.expired() && fixture.State->Destroyed == 1,
        "cancellation returns the exact ticket to its provider before releasing the last session");
  moved.Cancel();
  CHECK(fixture.State->Canceled == 1 && moved.Poll().Where() == SourceDelivery::State::Consumed,
        "terminal cancellation is idempotent and never restarts acquisition");
}

void PendingDestruction() {
  Fixture fixture;
  auto session = fixture.Open();
  CHECK(session.has_value(), "the destructor-owned session opens");
  if (!session) { return; }
  const std::weak_ptr<SourceSession> retained = *session;
  {
    auto read = SourceSession::Request(*session, Request(), fixture.Wire);
    CHECK(read.has_value(), "the destructor-owned input request selects its source");
    if (!read) { return; }
    session->reset();
    CHECK(read->Poll().Where() == SourceDelivery::State::Pending,
          "the provider owns pending IO before the input owner is destroyed");
  }
  CHECK(fixture.State->Canceled == 1 && fixture.State->Last == Ticket{1} &&
            fixture.Wire.Canceled == 0 && fixture.State->Destroyed == 1 && retained.expired(),
        "destroying the input owner cancels provider work before releasing its pinned session");
}

void MoveAssignment() {
  Fixture fixture;
  auto session = fixture.Open();
  CHECK(session.has_value(), "the move-assignment session opens");
  if (!session) { return; }
  auto first = SourceSession::Request(*session, Request(1), fixture.Wire);
  auto second = SourceSession::Request(*session, Request(2), fixture.Wire);
  CHECK(first && second, "two distinct input requests select their own provider tickets");
  if (!first || !second) { return; }
  CHECK(first->Poll().Where() == SourceDelivery::State::Pending &&
            second->Poll().Where() == SourceDelivery::State::Pending && fixture.State->Began == 2,
        "each input starts once before ownership replacement");
  *first = std::move(*second);
  CHECK(fixture.State->Canceled == 1 && fixture.State->Last == Ticket{1},
        "move assignment cancels the replaced destination before taking the source request");
  first->Cancel();
  second->Cancel();
  CHECK(fixture.State->Canceled == 2 && fixture.State->Last == Ticket{2} &&
            fixture.Wire.Canceled == 0,
        "the transferred provider ticket has exactly one cancellation owner");
}

void DeliveryAndEarlyDestruction() {
  Fixture fixture;
  auto session = fixture.Open();
  CHECK(session.has_value(), "the completed-delivery session opens");
  if (!session) { return; }
  {
    auto unused = SourceSession::Request(*session, Request(), fixture.Wire);
    CHECK(unused.has_value(), "an unpolled source request owns its declaration selection");
  }
  CHECK(fixture.State->Began == 0 && fixture.State->Canceled == 0,
        "destroying an unpolled request neither starts nor cancels foreign work");
  fixture.State->Ready = true;
  {
    auto ready = SourceSession::Request(*session, Request(), fixture.Wire);
    CHECK(ready.has_value(), "the ready request is admitted");
    if (!ready) { return; }
    auto delivered = ready->Poll();
    auto answer = delivered.Take();
    CHECK(answer && answer->Bytes == std::vector<uint8_t>({3, 5, 8}) &&
              answer->SourceId == "session-read-fixture" && answer->SourceRevision == "r1",
          "the owned request preserves the exact payload and source binding");
    CHECK(ready->Poll().Where() == SourceDelivery::State::Consumed,
          "completed input delivery is consumed once");
  }
  CHECK(fixture.State->Canceled == 0, "a completed provider ticket is never canceled again");
  auto absent = SourceSession::Request({}, Request(), fixture.Wire);
  CHECK(!absent && fixture.Wire.Began == 0, "a missing source session fails before any IO");
}
}

int main() {
  OwnershipAndMove();
  PendingDestruction();
  MoveAssignment();
  DeliveryAndEarlyDestruction();
  return Report();
}
