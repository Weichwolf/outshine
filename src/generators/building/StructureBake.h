#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREBAKE_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREBAKE_H

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "math/Vec3.h"
#include "BuildingField.h"
#include "HeightField.h"
#include "StructureMesher.h"

namespace outshine::Generators {

struct RawTile {
  struct Structure {
    uint32_t LocalFirst = 0;
    uint32_t PointCount = 0;
    uint32_t SourceFirst = 0;
    double HeightM = 0.0;
    int Pitched = -1;
  };

  struct Way {
    uint32_t LocalFirst = 0;
    uint32_t PointCount = 0;
    float HalfWidthM = 0.0f;
  };

  std::vector<double> LatLon;
  std::vector<Structure> Structures;
  std::vector<Way> Ways;
  Vec3 AnchorEcef;
  double AwayM = 0.0;
  double FocalPx = 0.0;
  double TileSpanM = 0.0;
  int Extent = 4096;
};

struct BakedTile {
  Raised Built;
  std::vector<outshine::Ground::BuildingField::Footprint> Prints;
  std::vector<double> SeatSpreadM;
  std::vector<double> AcrossM;
  int OsmHeights = 0;
  int DefaultHeights = 0;
  int Fronted = 0;
  int Lumped = 0;
  int Blocks = 0;
  int NoGround = 0;
};

void BakeStructures(const RawTile &raw,
                    const outshine::Ground::HeightField &heights,
                    const StructureMesher &mesher,
                    BakedTile &out);

} // namespace outshine::Generators
#endif
