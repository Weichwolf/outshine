#ifndef OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H

#include <vector>
#include <span>

#include "BuildingShape.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const En &enu) const noexcept;

  void Cover(std::span<const En> plan, std::vector<En> &tris) const;

  static std::vector<En> Widened(std::span<const En> ring, double byM);

private:
  const BuildingShape &Shape_;
};

}
#endif
