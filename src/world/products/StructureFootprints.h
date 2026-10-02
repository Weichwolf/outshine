#ifndef OUTSHINE_WORLD_PRODUCTS_STRUCTUREFOOTPRINTS_H
#define OUTSHINE_WORLD_PRODUCTS_STRUCTUREFOOTPRINTS_H

#include "BuildingHeightInterval.h"
#include "GeographicRing.h"
#include "SourceProvenance.h"
#include <cstdint>
#include <vector>

namespace outshine::Ground {

struct StructureFootprints {
  struct Structure {
    uint32_t First = 0, Count = 0;
    uint32_t FirstHole = 0, HoleCount = 0;
    BuildingHeightInterval Height;
    Data::SourceObjectId Source;
  };

  Data::ProductOrigin Origin;
  std::vector<double> LatLon;
  std::vector<GeographicRing> Rings;
  std::vector<Structure> Structures;
};

}

#endif
