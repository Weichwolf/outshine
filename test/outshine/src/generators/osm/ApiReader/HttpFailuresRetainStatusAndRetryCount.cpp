#include "OsmApiReader.h"
#include "Check.h"

#include <world/data/Transport.h>

#include <array>
#include <string>
#include <vector>
#include <limits>

namespace {
class RetryWire final : public outshine::Data::Transport {
public:
  int Status = 503;
  int Starts = 0;
  double ClockMs = 100;
  double RetryAfterS = 0;
  bool SucceedAfterFirst = false;
  std::vector<double> StartedAtMs;
  const std::string Xml = "<osm version='0.6'/>";

  outshine::Data::FetchStart Begin(const std::string &) override {
    StartedAtMs.push_back(ClockMs);
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    if (SucceedAfterFirst && Starts > 1) {
      return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(Xml.begin(), Xml.end()));
    }
    return outshine::Data::Wire::Answered(Status, {}, RetryAfterS);
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
  for (const double invalid : std::array{-1.0,
                                         std::numeric_limits<double>::infinity(),
                                         std::numeric_limits<double>::quiet_NaN()}) {
    RetryWire refused;
    refused.Status = 509;
    refused.RetryAfterS = invalid;
    const auto failed = Generators::Osm::ReadRegion(provider, store, refused, 20000, {});
    CHECK(!failed && refused.Starts == 1, "invalid quota delay cannot authorize a retry");
  }
  RetryWire cooldown;
  cooldown.Status = 509;
  cooldown.RetryAfterS = 213;
  cooldown.SucceedAfterFirst = true;
  const auto resumed = Generators::Osm::ReadRegion(provider, store, cooldown, 220000, {});
  CHECK(resumed && resumed->Xml == cooldown.Xml && cooldown.Starts == 2,
        "a quota with an explicit cooldown can resume original-data acquisition");
  CHECK(cooldown.StartedAtMs.size() == 2 &&
            cooldown.StartedAtMs[1] - cooldown.StartedAtMs[0] >= 213000,
        "server cooldown is not shortened to the scheduler's exponential backoff cap");
  RetryWire deadline;
  deadline.Status = 509;
  deadline.RetryAfterS = 213;
  const auto expired = Generators::Osm::ReadRegion(provider, store, deadline, 10000, {});
  CHECK(!expired && deadline.Starts == 1 && expired.error().find("deadline") != std::string::npos,
        "acquisition deadline cancels a cooldown without an early request");
  RetryWire exhausted;
  exhausted.Status = 509;
  exhausted.RetryAfterS = 213;
  const auto failed = Generators::Osm::ReadRegion(provider, store, exhausted, 1000000, {});
  CHECK(!failed && exhausted.Starts == 5 &&
            failed.error().find("HTTP 509 after 4 retries") != std::string::npos,
        "explicit cooldowns retain the bounded retry budget and terminal diagnostic");
  for (size_t at = 1; at < exhausted.StartedAtMs.size(); ++at) {
    CHECK(exhausted.StartedAtMs[at] - exhausted.StartedAtMs[at - 1] >= 213000,
          "every retry obeys its own server cooldown");
  }
  return Report();
}
