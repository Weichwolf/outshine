#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSTRUCTUREDESCRIPTION_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSTRUCTUREDESCRIPTION_H

#include "OsmBuildingFootprints.h"
#include "StructureFootprints.h"
#include "OsmSourceSnapshot.h"
#include <memory>

namespace outshine::Generators::Osm {

struct StructurePolicy {
  HeightPolicy Heights;
  double PointWidthM = 0.0;
  size_t PointsMost = 0;
};

enum class StructureDescriptionError : uint8_t {
  SourceMismatch,
  MissingReference,
  InvalidHeight,
  InvalidPointPolicy,
  InvalidCell,
  PointBudgetExceeded,
  UnassignedCourtyard
};

struct StructureDescription {
  outshine::Ground::StructureFootprints Footprints;
  std::shared_ptr<const Data::OsmSourceSnapshot> Source;
  std::weak_ptr<const Data::OsmSourceSnapshot> Archive;
};

[[nodiscard]] std::expected<StructureDescription, StructureDescriptionError>
DescribeStructures(const BuildingFootprints &buildings,
                   const std::shared_ptr<const Data::OsmSourceSnapshot> &source,
                   Data::ProductOrigin origin,
                   StructurePolicy policy);

}
#endif
