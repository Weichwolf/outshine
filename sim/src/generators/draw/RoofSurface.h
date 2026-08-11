/* THE ROOF AS A HEIGHT OVER THE PLAN, and the plan is the outline — never the box. The shape is
 * defined in the box frame, where a ridge is a straight line and a hip is a pair of them; the
 * triangles are cut from the OUTLINE and split along the shape's own creases, so the eaves sit on
 * the wall they belong to whatever the plan does, and every triangle lies flat in one facet.
 *
 * A gable end needs no case of its own: on a gable the height over the boundary is greater than zero
 * at the ends and zero along the sides, so the wall that closes it is whatever the boundary height
 * says. The same is true of a shed, a mansard and a sawtooth. */
#ifndef ROOFSURFACE_H
#define ROOFSURFACE_H

#include <vector>

#include "BuildingShape.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  /* Metres above the eaves, never below it. */
  double HeightAt(const Plan2 &enu) const noexcept;

  /* The plan, triangulated and cut along every crease. Three points per triangle, appended. */
  void Cover(const std::vector<Plan2> &plan, std::vector<Plan2> &tris) const;

  /* The outline pushed out by `byM`, or EMPTY where pushing it would fold it — a verge that turns
   * itself inside out is not a verge, and a caller has to be able to see that it got none. Negative
   * pushes inward, which is what a parapet's inner face is. */
  static std::vector<Plan2> Widened(const std::vector<Plan2> &ring, double byM);

private:
  const BuildingShape &Shape_;
};

}  // namespace outshine::Generators
#endif
