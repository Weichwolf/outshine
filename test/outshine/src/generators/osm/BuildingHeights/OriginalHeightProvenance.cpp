#include "OsmBuildingHeights.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <string>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const std::array station{outshine::Generators::Osm::Tag{"building", "train_station"},
                           outshine::Generators::Osm::Tag{"min_height", "5.5"},
                           outshine::Generators::Osm::Tag{"building:min_level", "1"}};
  const auto missing = outshine::Generators::Osm::BuildingHeights::Read(station);
  CHECK(missing.TopM && !*missing.TopM && missing.MinimumM && **missing.MinimumM == 5.5,
        "original station has missing top, not a fabricated five-metre height");
  const std::array explicitFive{outshine::Generators::Osm::Tag{"height", "5"},
                                outshine::Generators::Osm::Tag{"min_height", "5.5"}};
  const auto conflict = outshine::Generators::Osm::BuildingHeights::Read(explicitFive);
  CHECK(conflict.TopM && **conflict.TopM == 5 && **conflict.MinimumM == 5.5,
        "contradictory explicit heights remain intact for declared resolution policy");
  const std::array bridge{outshine::Generators::Osm::Tag{"building:levels", "1"},
                          outshine::Generators::Osm::Tag{"building:min_level", "2"}};
  const auto levels = outshine::Generators::Osm::BuildingHeights::Read(bridge);
  CHECK(!*levels.TopM && !*levels.MinimumM && **levels.Levels == 1 && **levels.MinimumLevel == 2,
        "levels are not silently converted into measured metric heights");
  const std::array units{outshine::Generators::Osm::Tag{"height", " 10 ft "},
                         outshine::Generators::Osm::Tag{"min_height", "1.2 m"}};
  const auto metric = outshine::Generators::Osm::BuildingHeights::Read(units);
  CHECK(metric.TopM && std::abs(**metric.TopM - 3.048) < 1e-12 && **metric.MinimumM == 1.2,
        "explicit units normalize to metres");
  for (const auto *text : {"", "nan", "inf", "-1", "3;4", "about 5", "4 km"}) {
    const std::array tags{outshine::Generators::Osm::Tag{"height", text}};
    CHECK(!outshine::Generators::Osm::BuildingHeights::Read(tags).TopM,
          "malformed height stays distinct from missing");
  }
  const std::array duplicate{outshine::Generators::Osm::Tag{"height", "5"},
                             outshine::Generators::Osm::Tag{"height", "5"}};
  const auto ambiguous = outshine::Generators::Osm::BuildingHeights::Read(duplicate);
  CHECK(!ambiguous.TopM &&
            ambiguous.TopM.error() == outshine::Generators::Osm::HeightError::DuplicateTag,
        "duplicate values do not acquire accidental first-tag precedence");
  return Report();
}
