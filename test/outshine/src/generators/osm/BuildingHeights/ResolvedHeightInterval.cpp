#include "OsmBuildingHeights.h"
#include "Check.h"

#include <array>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Data;
  using namespace outshine::Test;
  constexpr outshine::Generators::Osm::HeightPolicy policy{.StoreyHeightM = 3.0,
                                                           .BodyHeightM = 4.0};
  const std::array station{OsmTag{"min_height", "5.5"}};
  const auto raised = outshine::Generators::Osm::BuildingHeights::Read(station).Resolve(policy);
  CHECK(raised && raised->MinimumM == 5.5 && raised->TopM == 9.5 &&
            raised->TopOrigin == outshine::Ground::BuildingHeightOrigin::Generated &&
            raised->MinimumOrigin == outshine::Ground::BuildingHeightOrigin::Declared,
        "missing top uses declared body height above explicit minimum, not a measured top");
  const std::array bridge{OsmTag{"building:levels", "1"}, OsmTag{"building:min_level", "2"}};
  const auto ambiguous = outshine::Generators::Osm::BuildingHeights::Read(bridge).Resolve(policy);
  CHECK(ambiguous && ambiguous->MinimumM == 6.0 && ambiguous->TopM == 10.0 &&
            ambiguous->ConflictingLevels &&
            ambiguous->TopOrigin == outshine::Ground::BuildingHeightOrigin::Generated,
        "contradictory level interval remains diagnosed while policy supplies the body");
  const std::array levels{OsmTag{"building:levels", "4"}, OsmTag{"building:min_level", "2"}};
  const auto derived = outshine::Generators::Osm::BuildingHeights::Read(levels).Resolve(policy);
  CHECK(derived && derived->TopM == 12.0 && derived->MinimumM == 6.0 &&
            derived->TopOrigin == outshine::Ground::BuildingHeightOrigin::Storeys &&
            !derived->ConflictingLevels,
        "total levels include omitted lower levels rather than adding them twice");
  const std::array explicitHeight{OsmTag{"height", "20"}, OsmTag{"building:levels", "4"}};
  const auto measured =
      outshine::Generators::Osm::BuildingHeights::Read(explicitHeight).Resolve(policy);
  CHECK(measured && measured->TopM == 20.0 &&
            measured->TopOrigin == outshine::Ground::BuildingHeightOrigin::Declared,
        "explicit height overrides level-derived height");
  const std::array contradictoryLevels{OsmTag{"height", "20"},
                                       OsmTag{"min_height", "0"},
                                       OsmTag{"building:levels", "1"},
                                       OsmTag{"building:min_level", "2"}};
  const auto retained =
      outshine::Generators::Osm::BuildingHeights::Read(contradictoryLevels).Resolve(policy);
  CHECK(retained && retained->TopM == 20.0 && retained->MinimumM == 0.0 &&
            retained->ConflictingLevels,
        "valid metric interval does not erase contradictory source levels");
  const std::array conflict{OsmTag{"height", "5"}, OsmTag{"min_height", "5.5"}};
  const auto invalid = outshine::Generators::Osm::BuildingHeights::Read(conflict).Resolve(policy);
  CHECK(!invalid && invalid.error() == outshine::Generators::Osm::HeightError::InvalidInterval,
        "explicit contradictory heights cannot be silently repaired by adding a default");
  const std::array malformed{OsmTag{"height", "unknown"}};
  CHECK(!outshine::Generators::Osm::BuildingHeights::Read(malformed).Resolve(policy),
        "malformed height is not missing height");
  CHECK(!outshine::Generators::Osm::BuildingHeights::Read(station).Resolve(
            {.StoreyHeightM = 0.0, .BodyHeightM = 4.0}),
        "invalid policy cannot enter generated geometry");
  const std::array overflowing{OsmTag{"building:levels", "1e308"}};
  CHECK(!outshine::Generators::Osm::BuildingHeights::Read(overflowing).Resolve(policy),
        "finite source values cannot overflow into published geometry");
  return Report();
}
