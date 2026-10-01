#include "OsmApiReader.h"
#include "OfflineTransport.h"
#include "Sha256.h"
#include "SourceProviderValidation.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>

namespace {
std::string Xml(size_t id) {
  return "<osm version='0.6'><node id='" + std::to_string(id) +
         "' lat='0' lon='0'><tag k='custom:unknown' v='preserved'/></node></osm>";
}

class ApiWire final : public outshine::Data::Transport {
public:
  size_t Starts = 0, Peak = 0, Canceled = 0;
  double ClockMs = 0;
  bool Stall = false, Refuse = false;
  std::map<outshine::Data::Ticket, double> Active;

  outshine::Data::FetchStart Begin(const std::string &) override {
    const auto ticket = static_cast<outshine::Data::Ticket>(++Starts);
    Active.emplace(ticket, ClockMs + (Starts % 2 == 0 ? 5 : 10));
    Peak = std::max(Peak, Active.size());
    return ticket;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    const auto ready = Active.find(ticket);
    if (ready == Active.end()) { return outshine::Data::Wire::Working(); }
    if (Stall || ClockMs < ready->second) { return outshine::Data::Wire::Working(); }
    Active.erase(ready);
    const auto xml = Xml(static_cast<size_t>(ticket));
    return outshine::Data::Wire::Answered(Refuse ? 403 : 200,
                                          std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket ticket) override { Canceled += Active.erase(ticket); }

  double NowMs() override { return ClockMs; }

  bool Await(double ms) override {
    ClockMs += ms;
    return false;
  }
};
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-parallel-original-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-response cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  std::array<SourceProvider, 4> providers;
  for (size_t at = 0; at < providers.size(); ++at) {
    providers[at] = {.Kind = "osm",
                     .Revision = "adjacent-original-regions",
                     .Priority = static_cast<int>(at),
                     .Missing = MissingDataPolicy::Fail,
                     .Dataset = "openstreetmap.original",
                     .Endpoint = std::string(kOfficialOsmApi),
                     .Coverage = SourceCoverage{.WestDeg = static_cast<double>(at) * 0.01,
                                                .SouthDeg = 0,
                                                .EastDeg = static_cast<double>(at + 1) * 0.01,
                                                .NorthDeg = 0.01},
                     .PayloadSha256 = outshine::Sha256Hex(Xml(at + 1))};
  }
  ContentStore store({.Directory = directory});
  ApiWire wire;
  const auto cold = ReadOsmApiRegions(providers, store, wire, 100, {});
  CHECK(cold && cold->Chunks.size() == 4 && wire.Starts == 4 && wire.Peak == 2 &&
            wire.Active.empty() && cold->ElapsedMs == 15,
        "two bounded concurrent requests finish four delayed responses in half the serial time");
  double summedMs = 0;
  if (cold && cold->Chunks.size() == 4) {
    for (size_t at = 0; at < providers.size(); ++at) {
      const auto &chunk = cold->Chunks[at];
      CHECK(chunk.Provider == providers[at] && chunk.Xml == Xml(at + 1) && !chunk.FromStore,
            "out-of-order completions retain input order, original bytes and independent pins");
      summedMs += chunk.ReadMs;
    }
    CHECK(cold->ElapsedMs < summedMs, "elapsed acquisition time does not sum overlapping IO");
    const auto parsed = OsmChunkSetLoader::ParseRegion(cold->Chunks);
    CHECK(parsed && parsed->Chunks.size() == 4 && parsed->Elements.FindNode(4),
          "independently pinned responses merge as one consistent original dataset");
  }
  OfflineTransport offline;
  const auto warm = ReadOsmApiRegions(providers, store, offline, offline.NowMs() + 1000, {});
  CHECK(warm && warm->Chunks.size() == 4 &&
            std::ranges::all_of(warm->Chunks, [](const auto &chunk) { return chunk.FromStore; }),
        "all regions reload offline from original-response bytes");

  ContentStore noCache({.Using = ContentStore::Use::Off});
  ApiWire stalled;
  stalled.Stall = true;
  CHECK(!ReadOsmApiRegions(providers, noCache, stalled, 7, {}) && stalled.ClockMs == 7 &&
            stalled.Starts == 2 && stalled.Canceled == 2 && stalled.Active.empty(),
        "one deadline bounds the batch and cancels every outstanding ticket");
  ApiWire refused;
  refused.Refuse = true;
  CHECK(!ReadOsmApiRegions(providers, noCache, refused, 100, {}) && refused.Starts == 2 &&
            refused.Canceled == 1 && refused.Active.empty(),
        "a terminal region failure cancels siblings and does not launch queued regions");
  ApiWire canceled;
  std::stop_source stop;
  (void)stop.request_stop();
  CHECK(!ReadOsmApiRegions(providers, noCache, canceled, 100, stop.get_token()) &&
            canceled.Starts == 0,
        "an already canceled batch performs no acquisition");
  std::error_code ignored;
  (void)std::filesystem::remove_all(directory, ignored);
  return Report();
}
