#include "Check.h"
#include "DeclaredSources.h"
#include "OfflineTransport.h"
#include "OsmApiReader.h"
#include "Sha256.h"
#include "SourceProviderValidation.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <stop_token>
#include <string>
#include <system_error>

namespace {
std::string Xml(size_t version) {
  return "<osm version='0.6'><node id='1' lat='54.78' lon='9.43'/>"
         "<way id='10'><nd ref='1'/><tag k='building' v='yes'/>"
         "<tag k='custom:unknown' v='" +
         std::to_string(version) +
         "'/></way><relation id='20'><member type='way' ref='999' role='outer'/>"
         "<tag k='type' v='multipolygon'/></relation></osm>";
}

class ApiWire final : public outshine::Data::Transport {
public:
  size_t Starts = 0, Peak = 0, Canceled = 0;
  double ClockMs = 0;
  bool Stall = false, Refuse = false;
  std::stop_source *Stop = nullptr;
  std::map<outshine::Data::Ticket, double> Active;

  outshine::Data::FetchStart Begin(const std::string &) override {
    const auto ticket = static_cast<outshine::Data::Ticket>(++Starts);
    Active.emplace(ticket, ClockMs + (Starts % 2 == 0 ? 5 : 10));
    Peak = std::max(Peak, Active.size());
    return ticket;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    const auto ready = Active.find(ticket);
    if (ready == Active.end() || Stall || ClockMs < ready->second) {
      return outshine::Data::Wire::Working();
    }
    Active.erase(ready);
    const auto xml = Xml(static_cast<size_t>(ticket));
    return outshine::Data::Wire::Answered(Refuse ? 403 : 200,
                                          std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket ticket) override { Canceled += Active.erase(ticket); }

  double NowMs() override { return ClockMs; }

  bool Await(double ms) override {
    ClockMs += ms;
    if (Stop) { (void)Stop->request_stop(); }
    return false;
  }
};

void CheckSnapshots(const outshine::Data::OsmSourceRead &read,
                    std::span<const outshine::Data::GeoCellId> cells,
                    bool stored) {
  using namespace outshine::Data;
  using namespace outshine::Test;
  for (size_t at = 0; at < cells.size(); ++at) {
    const auto &chunk = read.Chunks[at];
    CHECK(chunk.Cell == cells[at] && chunk.Provider.Coverage == cells[at].Bounds() &&
              chunk.Xml == Xml(at + 1) && chunk.FromStore == stored,
          "completion order does not change cell identity, coverage or raw bytes");
    auto parsed = OsmChunkSetLoader::ParseCell(chunk);
    CHECK(parsed && parsed->Cell == cells[at] && parsed->SourceBytes == chunk.Xml.size() &&
              parsed->Chunks.front().PayloadSha256 == outshine::Sha256Hex(chunk.Xml) &&
              !parsed->Chunks.front().PinVerified && parsed->Chunks.front().FromStore == stored,
          "a cell owns its parsed data and measured source digest without claiming a pin");
    if (!parsed) { continue; }
    const std::array roots{OsmElementId{.Kind = OsmElementKind::Way, .Id = 10}};
    CHECK(parsed->Elements.FindWay(10)->Tags.back().Value == std::to_string(at + 1) &&
              parsed->Elements.FirstMissingReference() &&
              !parsed->Elements.FirstMissingReference(roots),
          "overlapping versions remain independent and foreign incomplete relations stay intact");
  }
  CHECK(!OsmChunkSetLoader::ParseRegion(read.Chunks),
        "cell answers cannot silently become a global merged region");
  auto wrong = read.Chunks.front();
  wrong.Cell = cells.back();
  CHECK(!OsmChunkSetLoader::ParseCell(wrong), "mismatched cell coverage cannot publish");
}
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-original-cell-jobs-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original cell-response store created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const SourceProvider provider{.Kind = "osm",
                                .Revision = "cell-job-source",
                                .Missing = MissingDataPolicy::Fail,
                                .Dataset = "openstreetmap.original",
                                .Endpoint = std::string(kOfficialOsmApi)};
  const std::array cells{GeoCellId{.Level = 9, .X = 269, .Y = 411},
                         GeoCellId{.Level = 9, .X = 270, .Y = 411}};
  ProviderRegistry registry;
  RegisterShippedProviders(registry);
  ContentStore store({.Directory = directory});
  ApiWire wire;
  const auto cold = ReadOsmApiCells(provider, cells, store, wire, 100, {}, &registry);
  CHECK(cold && cold->Chunks.size() == 2 && wire.Peak == 2 && wire.Active.empty() &&
            cold->ElapsedMs == 10,
        "one finite cell job uses two overlapping requests and wall-clock acquisition time");
  if (cold && cold->Chunks.size() == 2) { CheckSnapshots(*cold, cells, false); }
  OfflineTransport offline;
  const auto warm =
      ReadOsmApiCells(provider, cells, store, offline, offline.NowMs() + 1000, {}, &registry);
  CHECK(warm && warm->Chunks.size() == 2, "received cell bytes reload offline");
  if (warm && warm->Chunks.size() == 2) { CheckSnapshots(*warm, cells, true); }

  ContentStore uncached({.Using = ContentStore::Use::Off});
  const std::array excessive{cells[0], cells[1], GeoCellId{.Level = 9, .X = 271, .Y = 411}};
  const std::array duplicate{cells[0], cells[0]};
  const std::array invalid{cells[0], GeoCellId{.Level = 8}};
  ApiWire untouched;
  CHECK(!ReadOsmApiCells(provider, {}, uncached, untouched, 100, {}) &&
            !ReadOsmApiCells(provider, excessive, uncached, untouched, 100, {}) &&
            !ReadOsmApiCells(provider, duplicate, uncached, untouched, 100, {}) &&
            !ReadOsmApiCells(provider, invalid, uncached, untouched, 100, {}) &&
            untouched.Starts == 0,
        "empty, oversized, duplicate and invalid jobs are refused before network work");
  ApiWire stalled;
  stalled.Stall = true;
  CHECK(!ReadOsmApiCells(provider, cells, uncached, stalled, 7, {}) && stalled.ClockMs == 7 &&
            stalled.Canceled == 2 && stalled.Active.empty(),
        "a shared deadline cancels every unfinished cell without publishing a partial batch");
  ApiWire canceled;
  std::stop_source stop;
  canceled.Stop = &stop;
  CHECK(!ReadOsmApiCells(provider, cells, uncached, canceled, 100, stop.get_token()) &&
            canceled.Canceled == 2 && canceled.Active.empty(),
        "canceling a cell job releases both active transport tickets");
  ApiWire refused;
  refused.Refuse = true;
  CHECK(!ReadOsmApiCells(provider, cells, uncached, refused, 100, {}) && refused.Canceled == 1 &&
            refused.Active.empty(),
        "one refused cell abandons its pending neighbor without publishing a partial batch");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  return Report();
}
