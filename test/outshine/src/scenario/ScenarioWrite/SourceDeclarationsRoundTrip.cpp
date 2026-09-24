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
  source.Providers = {
      {.Kind = "terrain",
       .Revision = "snapshot & <a>\"\t",
       .Priority = std::numeric_limits<int>::min()},
      {.Kind = "vector",
       .Revision = "other'\n",
       .Priority = std::numeric_limits<int>::max(),
       .Missing = Data::MissingDataPolicy::Fail},
      {.Kind = "stars"},
      {.Kind = "osm",
       .Revision = "regional-r1",
       .Priority = 4,
       .Missing = Data::MissingDataPolicy::Fail,
       .Dataset = "openstreetmap",
       .Location = "osm/Hockenheim & region.osm",
       .Coverage = Data::SourceCoverage{
           .WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34}}};
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
      CHECK(a.Kind == b.Kind && a.Revision == b.Revision && a.Priority == b.Priority &&
                a.Missing == b.Missing && a.Dataset == b.Dataset && a.Location == b.Location &&
                a.Coverage == b.Coverage,
            "provider order and all fields preserved");
    }
  }
  CHECK(ReadScenario(text->data(), text->size(), copy, error) && error.empty(),
        "valid import retry succeeds");
  return Report();
}
