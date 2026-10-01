#include "Check.h"
#include "OsmApiReader.h"
#include "SourceProviderValidation.h"

#include <atomic>
#include <limits>
#include <string>
#include <vector>

namespace {
class DelayedWire final : public outshine::Data::Transport {
public:
  double ClockMs = 0;
  size_t Starts = 0, Canceled = 0;
  std::atomic<double> *Deadline = nullptr;
  double ChangedDeadlineMs = 0;

  outshine::Data::FetchStart Begin(const std::string &) override {
    ++Starts;
    return static_cast<outshine::Data::Ticket>(Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    if (ClockMs < 15) { return outshine::Data::Wire::Working(); }
    const std::string body = "<osm version='0.6'><node id='1' lat='0' lon='0'/></osm>";
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(body.begin(), body.end()));
  }

  void Cancel(outshine::Data::Ticket) override { ++Canceled; }

  double NowMs() override { return ClockMs; }

  bool Await(double ms) override {
    ClockMs += ms;
    if (Deadline && ClockMs >= 5) { Deadline->store(ChangedDeadlineMs); }
    return false;
  }
};
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  const std::vector<SourceProvider> providers{
      {.Kind = "osm",
       .Revision = "bounded",
       .Dataset = "openstreetmap.original",
       .Endpoint = std::string(kOfficialOsmApi),
       .Coverage = SourceCoverage{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1}}};
  ContentStore store({.Using = ContentStore::Use::Off});
  DelayedWire fixed;
  CHECK(!ReadOsmApiRegions(providers, store, fixed, 10, {}) && fixed.ClockMs == 10 &&
            fixed.Canceled == 1,
        "the fixed caller deadline stops and cancels acquisition before a late response");
  std::atomic<double> deadline{10};
  DelayedWire extended;
  extended.Deadline = &deadline;
  extended.ChangedDeadlineMs = 20;
  const auto changed = ReadOsmApiRegions(
      providers, store, extended, 10, {}, nullptr, {}, [&deadline] { return deadline.load(); });
  CHECK(changed && changed->Chunks.size() == 1 && extended.ClockMs == 15 && extended.Canceled == 0,
        "an updated preparation deadline reaches IO that was already pending");
  deadline = 20;
  DelayedWire shortened;
  shortened.Deadline = &deadline;
  shortened.ChangedDeadlineMs = 6;
  CHECK(!ReadOsmApiRegions(providers,
                           store,
                           shortened,
                           20,
                           {},
                           nullptr,
                           {},
                           [&deadline] { return deadline.load(); }) &&
            shortened.ClockMs == 6 && shortened.Canceled == 1,
        "a stricter caller budget cancels pending acquisition at the new bound");
  DelayedWire invalid;
  CHECK(!ReadOsmApiRegions(providers,
                           store,
                           invalid,
                           20,
                           {},
                           nullptr,
                           {},
                           [] { return std::numeric_limits<double>::infinity(); }) &&
            invalid.Starts == 0,
        "a nonfinite updated deadline cannot start unbounded acquisition");
  return Report();
}
