#include "OsmApiReader.h"
#include "Check.h"

#include <world/data/Transport.h>

#include <array>
#include <string>

namespace {
class RetryWire final : public outshine::Data::Transport {
public:
  int Status = 503;
  int Starts = 0;
  double ClockMs = 100;

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Answered(Status, {});
  }

  void Cancel(outshine::Data::Ticket) override {}

  bool Await(double ms) override {
    ClockMs += ms;
    return false;
  }

  double NowMs() override { return ClockMs; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Data::SourceProvider provider{
      .Kind = "osm",
      .Revision = "current",
      .Missing = Data::MissingDataPolicy::Fail,
      .Dataset = "openstreetmap.original",
      .Endpoint = "https://api.openstreetmap.org/api/0.6",
      .Coverage = Data::SourceCoverage{
          .WestDeg = 9.433, .SouthDeg = 54.785, .EastDeg = 9.445, .NorthDeg = 54.8}};
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  for (const int status : std::array{429, 503}) {
    RetryWire wire;
    wire.Status = status;
    const auto failed = Generators::Osm::ReadRegion(provider, store, wire, 20000, {});
    CHECK(!failed && wire.Starts == 5,
          "retryable HTTP failures exhaust exactly four retries after the first request");
    CHECK(!failed && failed.error().find("provider refusal HTTP " + std::to_string(status) +
                                         " after 4 retries") != std::string::npos,
          "HTTP status and exhausted retry count survive provider, scheduler and reader");
    CHECK(!failed &&
              failed.error().find("https://api.openstreetmap.org/api/0.6") != std::string::npos,
          "the terminal diagnostic identifies the original source");
  }
  RetryWire quota;
  quota.Status = 509;
  const auto limited = Generators::Osm::ReadRegion(provider, store, quota, 20000, {});
  CHECK(!limited && quota.Starts == 1,
        "an exhausted bandwidth quota does not trigger repeated downloads");
  CHECK(!limited &&
            limited.error().find("provider refusal HTTP 509 after 0 retries") != std::string::npos,
        "bandwidth refusal keeps its actual status and retry count");
  return Report();
}
