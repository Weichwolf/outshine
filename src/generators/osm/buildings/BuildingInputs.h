#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_BUILDINGINPUTS_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_BUILDINGINPUTS_H

#include "BuildingField.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StructureBake.h"
#include "TileAdmission.h"

#include <cstdint>
#include <optional>

namespace outshine::Generators::Osm {

void PrepareBuildingInputs(const OsmField &vectors,
                           const BuildingField &prints,
                           const StreetField &streets,
                           const TileAdmission::Next &next,
                           LongitudeLatitude eye,
                           std::optional<LevelOfDetail> detail,
                           std::optional<uint32_t> cell,
                           RawTile &raw);

}
#endif
