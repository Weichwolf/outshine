#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCEDEMAND_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCEDEMAND_H

#include "OsmSourceAcquisition.h"
#include "OsmXmlReader.h"
#include <cstddef>

namespace outshine::Generators::Osm {
inline constexpr int kCatalogueCellLevel = 9;
inline constexpr SourceAcquisition::CellLimits kDefaultCellLimits{
    .CellsMost = size_t{1} << static_cast<unsigned>(2 * kCatalogueCellLevel),
    .SnapshotBytesMost = 32 * kMaximumXmlBytes};
}
#endif
