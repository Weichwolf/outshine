#ifndef OUTSHINE_WORLD_GROUND_SURFACEMATERIAL_H
#define OUTSHINE_WORLD_GROUND_SURFACEMATERIAL_H

#include "Vec4.h"
#include <span>

namespace outshine::Ground {

struct SurfaceMaterial {
  Vec4f Ground;
  Vec4f Litter;
  Vec4f GroundSurf;
  Vec4f LitterSurf;
  Vec4f Mix;
  Vec4f Grass;
  Vec4f Dry;
  Vec4f Param;
  Vec4f Edge;
  int GroundClass = -1;

  [[nodiscard]] Vec4f MeanGround() const noexcept;
};

struct SurfacePaletteView {
  std::span<const SurfaceMaterial> Materials;
  int RockMaterial = 0;
  float SlopeBandDeg = 0.0f;
};

}
#endif
