#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <limits>
#include <cstddef>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double kWestDeg = 8.54;
  constexpr double kSouthDeg = 49.315;
  constexpr double kEastDeg = 8.61;
  constexpr double kNorthDeg = 49.34;
  Scenario::Document source;
  source.Providers = {{.Kind = "terrain",
                       .Revision = "snapshot & <a>\"\t",
                       .Priority = std::numeric_limits<int>::min(),
                       .Dataset = {},
                       .Location = {},
                       .Endpoint = {},
                       .Coverage = {},
                       .PayloadSha256 = {},
                       .Schema = {}},
                      {.Kind = "vector",
                       .Revision = "other'\n",
                       .Priority = std::numeric_limits<int>::max(),
                       .Missing = Data::MissingDataPolicy::Fail,
                       .Dataset = "tiles & revision",
                       .Location = {},
                       .Endpoint = "https://tiles.example/{z}/{x}/{y}?a=1&b=2",
                       .Coverage = {},
                       .PayloadSha256 = {},
                       .Schema = "schema & <revision>\"\t"},
                      {.Kind = "stars",
                       .Revision = {},
                       .Dataset = {},
                       .Location = {},
                       .Endpoint = {},
                       .Coverage = {},
                       .PayloadSha256 = {},
                       .Schema = {}},
                      {.Kind = "osm",
                       .Revision = "regional-r1",
                       .Priority = 4,
                       .Missing = Data::MissingDataPolicy::Fail,
                       .Dataset = "openstreetmap",
                       .Location = "osm/Hockenheim & region.osm",
                       .Endpoint = {},
                       .Coverage = Data::SourceCoverage{.WestDeg = kWestDeg,
                                                        .SouthDeg = kSouthDeg,
                                                        .EastDeg = kEastDeg,
                                                        .NorthDeg = kNorthDeg},
                       .PayloadSha256 = std::string(64, 'a'),
                       .Schema = {}}};
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
