#ifndef OUTSHINE_WORLD_PRODUCTS_BUILDINGFRONTAGE_H
#define OUTSHINE_WORLD_PRODUCTS_BUILDINGFRONTAGE_H

namespace outshine {

struct BuildingFrontage {
  bool Known = false;

  double KerbEm = 0.0, KerbNm = 0.0;
  double AlongE = 0.0, AlongN = 0.0;
  double ToStreetE = 0.0, ToStreetN = 0.0;

  [[nodiscard]] bool operator==(const BuildingFrontage &) const noexcept = default;
};

}

#endif
