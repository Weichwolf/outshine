#include "OsmProviderValidation.h"
#include "Check.h"
#include "OsmApiReader.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using outshine::GeographicBounds;

namespace {
class DelayedWire final : public outshine::Data::Transport {
public:
  double ClockMs = 0;
  size_t Starts = 0, Canceled = 0;
  std::atomic<double> *Deadline = nullptr;
  double ChangedDeadlineMs = 0;

  outshine::Data::FetchStart Begin(const std::string & /*url*/) override {
    ++Starts;
    return static_cast<outshine::Data::Ticket>(Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket /*ticket*/) override {
    if (ClockMs < 15) { return outshine::Data::Wire::Working(); }
    const std::string body = "<osm version='0.6'><node id='1' lat='0' lon='0'/></osm>";
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(body.begin(), body.end()));
  }

  void Cancel(outshine::Data::Ticket /*ticket*/) override { ++Canceled; }

  double NowMs() override { return ClockMs; }

  bool Await(double ms) override {
    ClockMs += ms;
    if ((Deadline != nullptr) && ClockMs >= 5) { Deadline->store(ChangedDeadlineMs); }
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
       .Missing = MissingDataPolicy::Fail,
       .Dataset = "openstreetmap.original",
       .Location = {},
       .Endpoint = std::string(outshine::Generators::Osm::kOfficialOsmApi),
       .Coverage =
           GeographicBounds{.WestDeg = -0.1, .SouthDeg = -0.1, .EastDeg = 0.1, .NorthDeg = 0.1},
       .PayloadSha256 = {},
       .Schema = {}}};
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
  DelayedWire fixed;
  CHECK(!outshine::Generators::Osm::ReadRegions(providers, store, fixed, 10, {}) &&
            fixed.ClockMs == 10 && fixed.Canceled == 1,
        "the fixed caller deadline stops and cancels acquisition before a late response");
  std::atomic<double> deadline{10};
  DelayedWire extended;
  extended.Deadline = &deadline;
  extended.ChangedDeadlineMs = 20;
  const auto changed = outshine::Generators::Osm::ReadRegions(
      providers, store, extended, 10, {}, nullptr, {}, [&deadline] { return deadline.load(); });
  CHECK(changed && changed->Chunks.size() == 1 && extended.ClockMs == 15 && extended.Canceled == 0,
        "an updated preparation deadline reaches IO that was already pending");
  deadline = 20;
  DelayedWire shortened;
  shortened.Deadline = &deadline;
  shortened.ChangedDeadlineMs = 6;
  CHECK(!outshine::Generators::Osm::ReadRegions(providers,
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
  CHECK(!outshine::Generators::Osm::ReadRegions(
            providers,
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
