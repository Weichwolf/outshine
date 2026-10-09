#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMBUILDINGFACADE_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMBUILDINGFACADE_H

#include "FacadeUv.h"
#include "OsmField.h"
#include "OsmBuildingFootprints.h"
#include <optional>
#include <span>
#include <string_view>

namespace outshine::Generators::Osm {
[[nodiscard]] std::optional<FacadeStyle> BuildingFacadeOf(std::string_view kind);
[[nodiscard]] std::optional<FacadeStyle> ReadBuildingFacade(const OsmField &field,
                                                            const OsmField::Feature &feature);
[[nodiscard]] std::optional<FacadeStyle> ReadBuildingFacade(std::span<const Tag> tags);
}
#endif
