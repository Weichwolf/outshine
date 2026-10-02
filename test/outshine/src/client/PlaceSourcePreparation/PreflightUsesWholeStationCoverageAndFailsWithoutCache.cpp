#include "src/client/PlaceSourcePreparation.h"
#include "OsmSourceDemand.h"
#include "ShippedProviders.h"
#include "Check.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {
class Wire final : public outshine::Data::Transport {
public:
  size_t Starts = 0;

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    const std::string xml = "<osm version='0.6'><node id='1' lat='0.1' lon='0.1'/></osm>";
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override {}
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-place-preflight-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated source cache exists");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  Scenario::Document declared;
  declared.Ground.Declared = true;
  declared.Ground.SightM = 100;
  declared.Ground.Origin = {.LatitudeDeg = 0.1, .LongitudeDeg = 0.1};
  declared.Providers.push_back({.Kind = "osm",
                                .Revision = "r1",
                                .Missing = Data::MissingDataPolicy::Fail,
                                .Dataset = "openstreetmap.original",
                                .Endpoint = "https://api.openstreetmap.org/api/0.6"});
  Scenario::View station;
  station.Placement = Scenario::CameraPlacement::Geodetic;
  station.Geographic.Geodetic = {.LongitudeDeg = 0.1, .LatitudeDeg = 0.1, .HeightM = 10};
  declared.Views = {station};
  Roots roots{.Shipped = ".", .Cache = directory, .Offline = true};
  const auto missing = Client::PreparePlaceSources(declared, roots, 2);
  CHECK(!missing && missing.error().find("OSM") != std::string::npos,
        "an offline missing source remains an error before any world or renderer is created");
  Data::ProviderRegistry registry;
  Generators::RegisterShippedProviders(registry);
  Tasks compute(1);
  Tasks io(1);
  Wire wire;
  const auto cells = Data::CellsAround(0.1,
                                       0.1,
                                       100,
                                       Generators::Osm::kCatalogueCellLevel,
                                       Generators::Osm::kDefaultCellLimits.CellsMost);
  CHECK(cells && cells->size() == 1, "the analytic station lies inside one source cell");
  if (!cells) { return Report(); }
  CHECK(Generators::Osm::PrepareSourceCache({.Catalogue = declared.Providers.front(),
                                             .Cells = *cells,
                                             .Limits = Generators::Osm::kDefaultCellLimits,
                                             .ShippedRoot = ".",
                                             .CacheDirectory = directory,
                                             .BudgetS = 2},
                                            {compute, io},
                                            wire,
                                            registry),
        "original network bytes populate the declared station cache");
  CHECK(Client::PreparePlaceSources(declared, roots, 2),
        "the actual client preflight completes from original cache bytes without network");
  declared.Views.front().Geographic.Geodetic.LatitudeDeg = 1.1;
  CHECK(!Client::PreparePlaceSources(declared, roots, 2),
        "camera coverage follows the station rather than the unchanged world anchor");
  declared.Views.front().Geographic.Geodetic.LatitudeDeg = 0.1;
  declared.Views.front().OffsetM = {{100000, 0, 0}};
  CHECK(!Client::PreparePlaceSources(declared, roots, 2),
        "a hundred kilometre camera offset requires its own geographic source coverage");
  declared.Views.front().OffsetM = {};
  declared.Ground.SightM = 240000;
  CHECK(!Client::PreparePlaceSources(declared, roots, 2),
        "nearby cached bytes cannot masquerade as complete declared 360 degree coverage");
  declared.Providers.clear();
  CHECK(Client::PreparePlaceSources(declared, roots, 2),
        "a scenario without an original catalogue needs no original-source preflight");
  std::filesystem::remove_all(directory);
  return Report();
}
