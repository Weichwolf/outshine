#include "OsmApiReader.h"
#include "OsmApiSource.h"
#include "OfflineTransport.h"
#include "SourceProviderValidation.h"
#include "OsmXmlReader.h"
#include "Sha256.h"
#include "Check.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace {
class ApiWire final : public outshine::Data::Transport {
public:
  std::string Xml = "<osm version='0.6'><node id='1' lat='54.79' lon='9.44'/>"
                    "<way id='1'><nd ref='1'/><tag k='man_made' v='chimney'/>"
                    "<tag k='height' v='53.5'/><tag k='custom:unknown' v='preserved'/></way>"
                    "<relation id='1'><member type='way' ref='1' role='outer'/>"
                    "<tag k='type' v='multipolygon'/></relation></osm>";
  std::vector<std::string> Urls;
  int Status = 200;
  int Canceled = 0;
  bool Stall = false;
  double ClockMs = 100;

  outshine::Data::FetchStart Begin(const std::string &url) override {
    Urls.push_back(url);
    return static_cast<outshine::Data::Ticket>(Urls.size());
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    if (Stall) { return outshine::Data::Wire::Working(); }
    return outshine::Data::Wire::Answered(Status, std::vector<uint8_t>(Xml.begin(), Xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override { ++Canceled; }

  bool Await(double ms) override {
    ClockMs += ms;
    return false;
  }

  double NowMs() override { return ClockMs; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-original-api-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-response cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  ContentStore store({.Directory = directory});
  ApiWire wire;
  SourceProvider provider{
      .Kind = "osm",
      .Revision = "r1",
      .Missing = MissingDataPolicy::Fail,
      .Dataset = "openstreetmap.original",
      .Endpoint = std::string(kOfficialOsmApi),
      .Coverage =
          SourceCoverage{.WestDeg = 9.433, .SouthDeg = 54.785, .EastDeg = 9.445, .NorthDeg = 54.8},
      .PayloadSha256 = Sha256Hex(wire.Xml)};
  const auto cold = ReadOsmApiRegion(provider, store, wire, 1100, {});
  CHECK(cold && cold->Xml == wire.Xml && !cold->FromStore &&
            wire.Urls ==
                std::vector<std::string>{
                    "https://api.openstreetmap.org/api/0.6/map?bbox=9.433,54.785,9.445,54.8"},
        "one official API request retains complete original response bytes and bbox order");
  if (cold) {
    const std::array input{*cold};
    const auto parsed = OsmChunkSetLoader::ParseRegion(input);
    CHECK(
        parsed && parsed->Elements.FindNode(1) && parsed->Elements.FindWay(1) &&
            parsed->Elements.FindRelation(1) && parsed->Elements.FindWay(1)->Tags.size() == 3 &&
            parsed->Chunks.size() == 1 && parsed->Chunks[0].PinVerified,
        "original object kinds, unknown tags and verified response provenance reach the snapshot");
  }
  OfflineTransport offline;
  const auto warm = ReadOsmApiRegion(provider, store, offline, offline.NowMs() + 1000, {});
  CHECK(cold && warm && warm->FromStore && warm->Xml == wire.Xml && warm->Origin == cold->Origin,
        "warm offline acquisition uses unchanged original bytes and official provenance");
  auto changed = provider;
  changed.PayloadSha256 = std::string(64, 'a');
  CHECK(!ReadOsmApiRegion(changed, store, offline, offline.NowMs() + 1000, {}),
        "a new expected response pin cannot reuse bytes under the old pin");
  for (const std::string endpoint : {"https://tiles.versatiles.org/api/0.6",
                                     "https://api.openstreetmap.org.evil.invalid/api/0.6",
                                     "http://api.openstreetmap.org/api/0.6"}) {
    changed = provider;
    changed.Endpoint = endpoint;
    CHECK(!OsmApiSource::Create(changed, 0), "unofficial or insecure endpoints fail before IO");
  }
  changed = provider;
  changed.Coverage = SourceCoverage{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1};
  CHECK(!OsmApiSource::Create(changed, 0),
        "API requests above the declared service area limit fail");
  ContentStore noCache({.Using = ContentStore::Use::Off});
  ApiWire stalled;
  stalled.Stall = true;
  CHECK(!ReadOsmApiRegion(provider, noCache, stalled, 110, {}) && stalled.Canceled == 1,
        "deadline cancels the outstanding transport ticket");
  std::stop_source stop;
  (void)stop.request_stop();
  ApiWire canceled;
  CHECK(!ReadOsmApiRegion(provider, noCache, canceled, 1100, stop.get_token()) &&
            canceled.Urls.empty(),
        "an already canceled request performs no network or cache acquisition");
  ApiWire oversized;
  oversized.Xml.assign(kMaxOsmXmlBytes + 1, ' ');
  CHECK(ReadOsmApiRegion(provider, store, oversized, 1100, {}) && oversized.Urls.empty(),
        "a warm request does not replace its cached bytes with a different wire payload");
  changed = provider;
  changed.Revision = "oversized-response";
  const auto writes = store.Counters().Writes;
  CHECK(!ReadOsmApiRegion(changed, store, oversized, 1100, {}) && store.Counters().Writes == writes,
        "oversized original responses are rejected before writing to the cache");
  ApiWire forbidden;
  forbidden.Status = 403;
  const auto absentWrites = store.Counters().Writes;
  changed.Revision = "forbidden-response";
  CHECK(!ReadOsmApiRegion(changed, store, forbidden, 1100, {}) &&
            store.Counters().Writes == absentWrites,
        "a forbidden response is not cached as original data or confirmed absence");
  auto api = OsmApiSource::Create(provider, 0);
  CHECK(api.has_value(), "official bounded source created for refusal classification");
  if (api) {
    ApiWire capacity;
    capacity.Status = 400;
    capacity.Xml = "You requested too many nodes (limit is 50000). Either request a smaller area, "
                   "or use planet.osm";
    auto rejected = (*api)->Collect(Address::Whole(0), static_cast<Ticket>(1), capacity).Take();
    CHECK(rejected && rejected->Reason == FetchFailureReason::CapacityRefused,
          "the official node-limit refusal is classified as capacity at the HTTP boundary");
    capacity.Xml = "The requested bounds are invalid";
    auto invalid = (*api)->Collect(Address::Whole(0), static_cast<Ticket>(1), capacity).Take();
    CHECK(invalid && invalid->Reason == FetchFailureReason::ProviderRefused,
          "an unrelated HTTP 400 cannot trigger subdivision");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated original-response cache removed");
  return Report();
}
