#ifndef OUTSHINE_GENERATORS_BUILDING_ORIGINALSTRUCTUREINPUT_H
#define OUTSHINE_GENERATORS_BUILDING_ORIGINALSTRUCTUREINPUT_H

#include "OsmBuildingFootprints.h"
#include "StructureBake.h"

namespace outshine::Generators {

struct OriginalStructurePolicy {
  outshine::Ground::OsmHeightPolicy Heights;
  double PointWidthM = 0.0;
  size_t PointsMost = 0;
};

enum class OriginalStructureInputError : uint8_t {
  SourceMismatch,
  MissingReference,
  InvalidHeight,
  InvalidPointPolicy,
  InvalidCell,
  PointBudgetExceeded,
  UnassignedCourtyard
};

[[nodiscard]] std::expected<RawTile, OriginalStructureInputError>
OriginalStructureInput(const outshine::Ground::OsmBuildingFootprints &buildings,
                       OriginalStructureSource source,
                       OriginalStructurePolicy policy);

}
#endif
