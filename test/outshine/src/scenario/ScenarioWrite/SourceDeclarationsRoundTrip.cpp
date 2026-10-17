#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Providers.resize(4);
  auto &terrain = source.Providers[0];
  terrain.Kind = "terrain";
  terrain.Revision = "snapshot & <a>\"\t";
  terrain.Priority = std::numeric_limits<int>::min();
  auto &vector = source.Providers[1];
  vector.Kind = "vector";
  vector.Revision = "other'\n";
  vector.Priority = std::numeric_limits<int>::max();
  vector.Missing = Data::MissingDataPolicy::Fail;
  vector.Dataset = "tiles & revision";
  vector.Endpoint = "https://tiles.example/{z}/{x}/{y}?a=1&b=2";
  vector.Schema = "schema & <revision>\"\t";
  source.Providers[2].Kind = "stars";
  auto &osm = source.Providers[3];
  osm.Kind = "osm";
  osm.Revision = "regional-r1";
  osm.Priority = 4;
  osm.Missing = Data::MissingDataPolicy::Fail;
  osm.Dataset = "openstreetmap";
  osm.Location = "osm/Hockenheim & region.osm";
  osm.Coverage =
      Data::SourceCoverage{.WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34};
  osm.PayloadSha256 = std::string(64, 'a');
  const auto text = WriteScenario(source);
  CHECK(text.has_value(), "export succeeds");
  if (!text) { return Report(); }
  Scenario::Document copy;
  std::string error;
  CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
  CHECK(copy.Providers.size() == source.Providers.size(), "provider count preserved");
  if (copy.Providers.size() == source.Providers.size()) {
    for (size_t i = 0; i < source.Providers.size(); ++i) {
      const auto &a = source.Providers[i];
      const auto &b = copy.Providers[i];
      CHECK(a == b, "provider order and all fields preserved");
    }
  }
  CHECK(ReadScenario(text->data(), text->size(), copy, error) && error.empty(),
        "valid import retry succeeds");
  return Report();
}
