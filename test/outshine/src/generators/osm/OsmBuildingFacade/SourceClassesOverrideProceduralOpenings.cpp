#include "OsmBuildingFacade.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array aliases{Tag{"class", "industrial"}, Tag{"subclass", "apartments"}};
  CHECK(ReadBuildingFacade(aliases) == FacadeStyle::Block,
        "specific source subclass beats the generic normalized class");
  const std::array declared{Tag{"building", "warehouse"}, Tag{"subclass", "apartments"}};
  CHECK(ReadBuildingFacade(declared) == FacadeStyle::Hall,
        "explicit building use overrides a normalized alias");
  const std::array chimney{Tag{"building", "yes"}, Tag{"man_made", "chimney"}};
  CHECK(ReadBuildingFacade(chimney) == FacadeStyle::Tower,
        "a known chimney never receives residential openings");
  const std::array normalized{Tag{"kind", "chimney"}};
  CHECK(ReadBuildingFacade(normalized) == FacadeStyle::Tower,
        "normalized vector tile classes retain service building semantics");
  const std::array unknown{Tag{"building", "yes"}, Tag{"class", "building"}};
  CHECK(!ReadBuildingFacade(unknown), "unknown use remains distinguishable from supplied use");
  CHECK(BuildingFacadeOf("house") == FacadeStyle::House &&
            BuildingFacadeOf("greenhouse") == FacadeStyle::Glazing &&
            BuildingFacadeOf("garage") == FacadeStyle::Outbuilding,
        "housing, glazing and closed service buildings retain independent opening families");
  return Report();
}
