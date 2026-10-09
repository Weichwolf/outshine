#ifndef OUTSHINE_GENERATORS_GROUNDSNAPSHOT_H
#define OUTSHINE_GENERATORS_GROUNDSNAPSHOT_H

#include <memory>

#include "ClassStructure.h"
#include "Ground.h"
#include "GroundTable.h"
#include "BuildingField.h"
#include "OsmField.h"
#include "StreetField.h"
#include "WaterAsset.h"
#include "Tile.h"
#include "GroundQuery.h"
#include "TerrainLoader.h"

namespace outshine::Generators {

enum class Snapped { Taken, Waiting, NoGround };

struct Fields {
  const ::outshine::Generators::Osm::OsmField *Vectors = nullptr;
  const ::outshine::Generators::Osm::BuildingField *Footprints = nullptr;
  const WaterAsset *WaterBodies = nullptr;
  const ::outshine::Generators::Osm::StreetField *Ways = nullptr;
  int BuiltRow = -1;
  int WetRow = -1;
};

[[nodiscard]] std::shared_ptr<const FeatureField> FeaturesOver(const Tile &region,
                                                               const Fields &stands);

[[nodiscard]] std::shared_ptr<const GroundPatch>
PatchOver(const Tile &region, const outshine::GroundQuery &heights, Snapped *how);

[[nodiscard]] int PatchSide(const Tile &region, const outshine::GroundQuery &heights);

[[nodiscard]] Snapped SnapshotOver(const Tile &region,
                                   const outshine::GroundQuery &heights,
                                   std::shared_ptr<const ClassStructure> classes,
                                   const Fields &stands,
                                   std::shared_ptr<const GroundTable> table,
                                   Ground::Snapshot *out,
                                   std::shared_ptr<const GroundPatch> patch = {});

[[nodiscard]] std::shared_ptr<const GroundTable>
TableOf(const outshine::Ground::VegetationTemplates &templates);

}

#endif
