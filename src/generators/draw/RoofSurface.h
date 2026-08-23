#ifndef OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H

#include <vector>
#include <span>

#include "BuildingShape.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const Plan2 &enu) const noexcept;

  void Cover(std::span<const Plan2> plan, std::vector<Plan2> &tris) const;

  static std::vector<Plan2> Widened(std::span<const Plan2> ring, double byM);

private:
  const BuildingShape &Shape_;
};

}
#endif
