#ifndef ROOFSURFACE_H
#define ROOFSURFACE_H

#include <vector>

#include "BuildingShape.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  double HeightAt(const Plan2 &enu) const noexcept;

  void Cover(const std::vector<Plan2> &plan, std::vector<Plan2> &tris) const;

  static std::vector<Plan2> Widened(const std::vector<Plan2> &ring, double byM);

private:
  const BuildingShape &Shape_;
};

}
#endif
