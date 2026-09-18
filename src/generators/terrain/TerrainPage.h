#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINPAGE_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINPAGE_H

#include <cstddef>

#include "GroundMesher.h"

namespace outshine::Generators {

struct TerrainPageLayout {
  int Side = 0;
  int Halo = 0;

  [[nodiscard]] bool Valid() const noexcept;
  [[nodiscard]] size_t PageSide() const noexcept;
  [[nodiscard]] size_t NodeCount() const noexcept;
  [[nodiscard]] size_t NodeAt(int column, int row) const noexcept;
  [[nodiscard]] double FractionAt(const Sheet &sheet, int node) const noexcept;
};

}
#endif
