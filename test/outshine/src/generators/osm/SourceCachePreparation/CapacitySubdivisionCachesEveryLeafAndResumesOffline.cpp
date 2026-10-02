#include "OsmSourceCachePreparation.h"
#include "OsmProvider.h"
#include "OfflineTransport.h"
#include "SourceProviderValidation.h"
#include "Check.h"

#include <array>
#include <atomic>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {
class CapacityWire final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0};
  std::map<outshine::Data::Ticket, std::array<double, 4>> Pending;

  outshine::Data::FetchStart Begin(const std::string &url) override {
    std::array<double, 4> bounds{};
    const auto offset = url.find("bbox=");
    if (offset == std::string::npos) {
      return std::unexpected(outshine::Data::FetchFailureReason::InvalidRequest);
    }
    const char *at = url.data() + offset + 5;
    for (auto &coordinate : bounds) {
      const auto parsed = std::from_chars(at, url.data() + url.size(), coordinate);
      if (parsed.ec != std::errc{}) {
        return std::unexpected(outshine::Data::FetchFailureReason::InvalidRequest);
      }
      at = parsed.ptr;
      if (at != url.data() + url.size()) { ++at; }
    }
    const auto ticket = static_cast<outshine::Data::Ticket>(++Starts);
    Pending.emplace(ticket, bounds);
    return ticket;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    const auto bounds = Pending.at(ticket);
    Pending.erase(ticket);
    const bool parent = bounds[2] - bounds[0] > 0.4;
    const std::string xml = parent ? "You requested too many nodes (limit is 50000). Either "
                                     "request a smaller area, or use planet.osm"
                                   : "<osm version='0.6'><node id='" +
                                         std::to_string(static_cast<uint64_t>(ticket)) + "' lat='" +
                                         std::to_string((bounds[1] + bounds[3]) / 2) + "' lon='" +
                                         std::to_string((bounds[0] + bounds[2]) / 2) + "'/></osm>";
    return outshine::Data::Wire::Answered(parent ? 400 : 200,
                                          std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket ticket) override { Pending.erase(ticket); }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-cache-refinement-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original cache exists");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const Data::SourceProvider catalogue{.Kind = "osm",
                                       .Revision = "adaptive-cache",
                                       .Missing = Data::MissingDataPolicy::Fail,
                                       .Dataset = "openstreetmap.original",
                                       .Endpoint = std::string(Data::kOfficialOsmApi)};
  const std::array roots{Data::GeoCellId{.Level = 9, .X = 274, .Y = 390}};
  const Generators::Osm::SourceCacheRequest request{
      .Catalogue = catalogue,
      .Cells = roots,
      .Limits = {.CellsMost = 16, .SnapshotBytesMost = 1024 * 1024},
      .ShippedRoot = ".",
      .CacheDirectory = directory,
      .BudgetS = 2};
  Generators::Osm::Provider provider;
  Data::ProviderRegistry registry;
  CHECK(registry.registerProvider(provider), "original source uses the public registry");
  Tasks compute(1);
  Tasks io(1);
  CapacityWire wire;
  Generators::Osm::SourceCacheProgress complete;
  CHECK(Generators::Osm::PrepareSourceCache(
            request,
            {compute, io},
            wire,
            registry,
            [&](Generators::Osm::SourceCacheProgress how) { complete = how; }),
        "actual API capacity rejection replaces one parent by fully verified child coverage");
  CHECK(wire.Starts == 5 && complete.ValidatedCells == 4 && complete.RequiredCells == 4,
        "exactly one parent probe and all four distinct child replies complete the whole demand");
  Data::OfflineTransport offline;
  complete = {};
  CHECK(Generators::Osm::PrepareSourceCache(
            request,
            {compute, io},
            offline,
            registry,
            [&](Generators::Osm::SourceCacheProgress how) { complete = how; }),
        "a fresh offline preparer reconstructs the completed partition from original child bytes");
  CHECK(complete.ValidatedCells == 4 && complete.RequiredCells == 4 && wire.Starts == 5,
        "warm preparation needs no parent network request and preserves every required leaf");
  std::filesystem::remove_all(directory);
  return Report();
}
