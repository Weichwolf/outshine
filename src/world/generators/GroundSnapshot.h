#ifndef OUTSHINE_WORLD_GENERATORS_GROUNDSNAPSHOT_H
#define OUTSHINE_WORLD_GENERATORS_GROUNDSNAPSHOT_H

#include <memory>

#include "ClassField.h"
#include "Ground.h"
#include "GroundTable.h"
#include "BuildingField.h"
#include "OsmField.h"
#include "StreetField.h"
#include "WaterField.h"
#include "Region.h"
#include "GroundQuery.h"
#include "TerrainLoader.h"

namespace outshine::Generators {

enum class Snapped { Taken, Waiting, NoGround };

// WHAT A FEATURE FIELD IS BUILT FROM, as a value. `Sim` owned these three fields and their two
// cover rows as members, which is why the whole generator tier hung off a class with one
// consumer. They are each built from a `GroundQuery` and an `OsmField` already; naming them here
// is what lets anything that opened a ground stack run a generator.
struct Standing {
  const outshine::Ground::OsmField *Vectors = nullptr;
  const outshine::Ground::BuildingField *Footprints = nullptr;
  const outshine::Ground::WaterField *WaterBodies = nullptr;
  const outshine::Ground::StreetField *Ways = nullptr;
  int BuiltRow = -1;
  int WetRow = -1;
};

[[nodiscard]] std::shared_ptr<const FeatureField> FeaturesOver(const Region &region,
                                                               const Standing &stands);

[[nodiscard]] Snapped SnapshotOver(const Region &region,
                                   const outshine::GroundQuery &heights,
                                   const outshine::Ground::ClassField &classes,
                                   const Standing &stands,
                                   std::shared_ptr<const GroundTable> table,
                                   Ground::Snapshot *out);

[[nodiscard]] std::shared_ptr<const GroundTable> TableOf(
    const outshine::Ground::VegetationTemplates &templates);

}

#endif
