#include "RoadLaneColour.h"
#include "RegionSurfaceRecipes.h"
#include "Vec4.h"
#include <cstddef>

namespace outshine::Generators {
Vec3f RoadLaneColour(int coverRow, const RegionSurfaceRecipes &vegetation) {
  if (coverRow < 0 || static_cast<size_t>(coverRow) >= vegetation.TemplateCount()) {
    return {{0.5f, 0.5f, 0.5f}};
  }
  const Vec4f &cover = vegetation.Rows()[static_cast<size_t>(coverRow)].Ground;
  return {{cover[0], cover[1], cover[2]}};
}
}
