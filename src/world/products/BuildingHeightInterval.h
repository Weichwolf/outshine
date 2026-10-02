#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGHEIGHTINTERVAL_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGHEIGHTINTERVAL_H

#include <cstdint>

namespace outshine::Ground {

enum class BuildingHeightOrigin : uint8_t { Declared, Storeys, Generated };

struct BuildingHeightInterval {
  double TopM;
  double MinimumM;
  BuildingHeightOrigin TopOrigin;
  BuildingHeightOrigin MinimumOrigin;
  bool ConflictingLevels;
};

}

#endif
