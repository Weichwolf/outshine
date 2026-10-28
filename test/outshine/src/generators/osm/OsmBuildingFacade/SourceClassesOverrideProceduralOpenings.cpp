#include "OsmBuildingFacade.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array aliases{Tag{.Key = "class", .Value = "industrial"},
                           Tag{.Key = "subclass", .Value = "apartments"}};
  CHECK(ReadBuildingFacade(aliases) == FacadeStyle::Block,
        "specific source subclass beats the generic normalized class");
  const std::array declared{Tag{.Key = "building", .Value = "warehouse"},
                            Tag{.Key = "subclass", .Value = "apartments"}};
  CHECK(ReadBuildingFacade(declared) == FacadeStyle::Hall,
        "explicit building use overrides a normalized alias");
  const std::array chimney{Tag{.Key = "building", .Value = "yes"},
                           Tag{.Key = "man_made", .Value = "chimney"}};
  CHECK(ReadBuildingFacade(chimney) == FacadeStyle::Tower,
        "a known chimney never receives residential openings");
  const std::array normalized{Tag{.Key = "kind", .Value = "chimney"}};
  CHECK(ReadBuildingFacade(normalized) == FacadeStyle::Tower,
        "normalized vector tile classes retain service building semantics");
  const std::array unknown{Tag{.Key = "building", .Value = "yes"},
                           Tag{.Key = "class", .Value = "building"}};
  CHECK(!ReadBuildingFacade(unknown), "unknown use remains distinguishable from supplied use");
  CHECK(BuildingFacadeOf("house") == FacadeStyle::House &&
            BuildingFacadeOf("greenhouse") == FacadeStyle::Glazing &&
            BuildingFacadeOf("garage") == FacadeStyle::Outbuilding,
        "housing, glazing and closed service buildings retain independent opening families");
  CHECK(BuildingFacadeOf("office") == FacadeStyle::Commercial &&
            BuildingFacadeOf("school") == FacadeStyle::Institutional &&
            BuildingFacadeOf("church") == FacadeStyle::Religious,
        "supplied uses retain distinct facade rhythms rather than one residential pattern");
  return Report();
}
