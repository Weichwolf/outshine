#include "Check.h"
#include "OsmCellAcquisition.h"
#include "SourceProviderValidation.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace {
class DelayedOsmTransport final : public outshine::Data::Transport {
public:
  int Starts = 0, Canceled = 0;
  bool BlockFirst = true, CapacityRefusal = false;

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    if (BlockFirst && ticket == static_cast<outshine::Data::Ticket>(1)) {
      return outshine::Data::Wire::Working();
    }
    const std::string xml = CapacityRefusal
                                ? "You requested too many nodes (limit is 50000). Either request a "
                                  "smaller area, or use planet.osm"
                                : "<osm version='0.6'><node id='1' lat='0' lon='0'><tag "
                                  "k='custom:unknown' v='kept'/></node></osm>";
    return outshine::Data::Wire::Answered(CapacityRefusal ? 400 : 200,
                                          std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override { ++Canceled; }
};
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-independent-osm-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    ContentStore store({.Directory = directory, .UtcSeconds = {}});
    const SourceProvider provider{.Kind = "osm",
                                  .Revision = "independent",
                                  .Missing = MissingDataPolicy::Fail,
                                  .Dataset = "openstreetmap.original",
                                  .Endpoint = std::string(kOfficialOsmApi)};
    DelayedOsmTransport wire;
    std::array<GeoCellId, OsmCellAcquisition::MaximumPendingCells> cells{};
    OsmCellAcquisition reader(provider, store, wire);
    for (size_t index = 0; index < cells.size(); ++index) {
      cells[index] = {.Level = 9, .X = static_cast<uint32_t>(256 + index), .Y = 256};
      CHECK(reader.Start(cells[index]), "each independent cell enters bounded acquisition");
    }
    CHECK(wire.Starts == 8 && !reader.Start({.Level = 9, .X = 300, .Y = 256}) &&
              reader.PendingCount() == cells.size(),
          "pending capacity rejects additional work before any transfer can begin");
    for (size_t index = 1; index < cells.size(); ++index) {
      auto ready = reader.TakeReady();
      CHECK(ready && *ready && (**ready).Chunks.size() == 1 &&
                (**ready).Chunks.front().Cell == cells[index] &&
                (**ready).Chunks.front().Xml.find("custom:unknown") != std::string::npos,
            "a completed original cell is transferred without waiting for the first request");
    }
    auto pending = reader.TakeReady();
    CHECK(pending && !*pending && reader.PendingCount() == 1 && wire.Starts == 8,
          "the delayed neighbor remains independently pending");
    wire.BlockFirst = false;
    auto last = reader.TakeReady();
    CHECK(last && *last && (**last).Chunks.front().Cell == cells.front() &&
              reader.PendingCount() == 0,
          "the delayed cell completes without losing its original address");
    OsmCellAcquisition warm(provider, store, wire);
    for (const auto cell : cells) {
      CHECK(warm.Start(cell), "cached cells enter the same acquisition path");
    }
    for (size_t index = 0; index < cells.size(); ++index) {
      auto ready = warm.TakeReady();
      CHECK(ready && *ready && (**ready).Chunks.front().FromStore && wire.Starts == 8,
            "warm original cells are served without additional network transfers");
    }
    wire.CapacityRefusal = true;
    const GeoCellId crowded{.Level = 9, .X = 301, .Y = 256};
    CHECK(reader.Start(crowded), "capacity-limited cell enters acquisition");
    auto refused = reader.TakeReady();
    CHECK(refused && *refused && (**refused).Chunks.empty() &&
              (**refused).Refine == std::vector{crowded},
          "capacity refusal preserves the address for complete subdivision rather than empty "
          "geometry");
    DelayedOsmTransport stopped;
    {
      OsmCellAcquisition canceled(provider, store, stopped);
      CHECK(canceled.Start({.Level = 9, .X = 302, .Y = 256}),
            "cancelable original cell enters acquisition");
      auto waiting = canceled.TakeReady();
      CHECK(waiting && !*waiting && stopped.Starts == 1, "cancelable request is truly pending");
    }
    CHECK(stopped.Canceled == 1, "query ownership cancels the pending transfer exactly once");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source cache removed");
  return Report();
}
